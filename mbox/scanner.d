module mbox.scanner;

import std.ascii : toLower;
import std.exception : enforce;
import std.stdio : File;

// All offsets are half-open byte ranges in the original file. The scanner never
// decodes or changes message bodies. A later Android adapter can call scan with
// an already opened descriptor without changing the framing implementation.
struct MessageRecord {
    ulong raw_start;
    ulong raw_end;
    ulong separator_end;
    ulong header_end;
    ulong body_start;
    string sender;
    string recipients;
    string subject;
    string date;
    bool possible_boundary;
    uint rejected_candidates;
}

private enum State { first, headers, body, pending }
private enum max_line_length = 1024 * 1024;
private enum max_header_bytes = 64 * 1024;

private bool digit(ubyte c) { return c >= '0' && c <= '9'; }

private int decimal(scope const(ubyte)[] part) {
    if (part.length == 0 || part.length > 4) return -1;
    int n;
    foreach (c; part) {
        if (!digit(c)) return -1;
        n = n * 10 + c - '0';
    }
    return n;
}

private bool among(scope const(ubyte)[] word, scope const(string)[] choices) {
    foreach (choice; choices) if (word == cast(const(ubyte)[]) choice) return true;
    return false;
}

// Deliberately conservative: accepting every line starting with "From " would
// silently split message bodies. Even a plausible postmark remains ambiguous;
// the output records that uncertainty for every later boundary.
private bool postmark(scope const(ubyte)[] line) {
    if (line.length == 0 || line[$ - 1] != '\n') return false;
    if (line.length >= 2 && line[$ - 2] == '\r') line = line[0 .. $ - 2];
    else line = line[0 .. $ - 1];
    if (line.length < 20 || line[0 .. 5] != cast(const(ubyte)[]) "From ") return false;

    size_t cursor = 5;
    while (cursor < line.length && line[cursor] != ' ') cursor++;
    if (cursor == 5 || cursor == line.length) return false;
    // The separator's envelope sender is distinct from an RFC From: header.
    cursor++;
    while (cursor < line.length && line[cursor] == ' ') cursor++;
    if (cursor + 3 >= line.length) return false;
    auto weekday = line[cursor .. cursor + 3];
    if (!among(weekday, ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"])) return false;
    cursor += 3;
    if (cursor >= line.length || line[cursor++] != ' ') return false;
    if (cursor + 3 >= line.length) return false;
    auto month = line[cursor .. cursor + 3];
    if (!among(month, ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"])) return false;
    cursor += 3;
    if (cursor >= line.length || line[cursor++] != ' ') return false;
    while (cursor < line.length && line[cursor] == ' ') cursor++;
    auto day_start = cursor;
    while (cursor < line.length && digit(line[cursor])) cursor++;
    auto day = decimal(line[day_start .. cursor]);
    if (day < 1 || day > 31 || cursor >= line.length || line[cursor++] != ' ') return false;
    if (cursor + 8 > line.length) return false;
    auto time = line[cursor .. cursor + 8];
    if (time[2] != ':' || time[5] != ':' ||
        decimal(time[0 .. 2]) > 23 || decimal(time[0 .. 2]) < 0 ||
        decimal(time[3 .. 5]) > 59 || decimal(time[3 .. 5]) < 0 ||
        decimal(time[6 .. 8]) > 60 || decimal(time[6 .. 8]) < 0) return false;
    cursor += 8;
    if (cursor >= line.length || line[cursor++] != ' ') return false;
    // Historical mbox postmarks sometimes include a timezone before the year.
    while (cursor < line.length && line[cursor] == ' ') cursor++;
    auto token_start = cursor;
    while (cursor < line.length && line[cursor] != ' ') cursor++;
    if (decimal(line[token_start .. cursor]) >= 1900 && cursor == line.length) return true;
    if (cursor == line.length) return false;
    while (cursor < line.length && line[cursor] == ' ') cursor++;
    return cursor + 4 == line.length && decimal(line[cursor .. $]) >= 1900;
}

private bool blank(scope const(ubyte)[] line) {
    return line == cast(const(ubyte)[]) "\n" || line == cast(const(ubyte)[]) "\r\n";
}

private bool field(scope const(ubyte)[] line) {
    size_t i;
    while (i < line.length && line[i] != ':') {
        const c = line[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-')) return false;
        i++;
    }
    return i > 0 && i < line.length && line[i] == ':';
}

private string clean(scope const(ubyte)[] value) {
    auto result = new char[value.length];
    foreach (i, c; value) result[i] = (c < 32 || c == 127 || c >= 128) ? ' ' : cast(char)c;
    size_t start;
    while (start < result.length && result[start] == ' ') start++;
    size_t end = result.length;
    while (end > start && result[end - 1] == ' ') end--;
    return result[start .. end].idup;
}

// Only selected short headers are copied. Raw bytes and offsets remain intact.
private string remember(ref MessageRecord record, scope const(ubyte)[] line) {
    size_t colon;
    while (colon < line.length && line[colon] != ':') colon++;
    auto name = new char[colon];
    foreach (i, c; line[0 .. colon]) name[i] = toLower(cast(char)c);
    auto value = clean(line[colon + 1 .. $]);
    switch (name) {
        case "from": record.sender = value; return "from";
        case "to": record.recipients = value; return "to";
        case "subject": record.subject = value; return "subject";
        case "date": record.date = value; return "date";
        default: return "";
    }
}

// A streaming, read-only postmark scan. It accepts RFC 4155 and common mboxrd
// postmarks but does not assume which body quoting dialect produced the file.
// Archives carrying Content-Length need a different framing mode and fail here.
void scan(ref File source, scope void delegate(in MessageRecord) emit) {
    auto state = State.first;
    MessageRecord record;
    ulong position;
    ulong line_start;
    ulong pending_start;
    ulong pending_end;
    size_t header_bytes;
    string last_field;
    auto buffer = new ubyte[64 * 1024];
    auto line = new ubyte[max_line_length];
    size_t line_length;

    void start(ulong start_offset, ulong separator_end, bool uncertain) {
        record = MessageRecord.init;
        record.raw_start = start_offset;
        record.separator_end = separator_end;
        record.possible_boundary = uncertain;
        header_bytes = 0;
        last_field = "";
        state = State.headers;
    }

    void header(scope const(ubyte)[] bytes, ulong start_offset, ulong end_offset) {
        header_bytes += bytes.length;
        enforce(header_bytes <= max_header_bytes, "mbox-index: header block exceeds 64 KiB at byte " ~ start_offset.toString);
        if (blank(bytes)) {
            record.header_end = start_offset;
            record.body_start = end_offset;
            state = State.body;
            return;
        }
        if (bytes[0] == ' ' || bytes[0] == '\t') {
            // RFC header folding belongs to the header value, never to the
            // byte offsets or to the mailbox framing decision.
            auto continuation = clean(bytes);
            switch (last_field) {
                case "from": record.sender ~= " " ~ continuation; break;
                case "to": record.recipients ~= " " ~ continuation; break;
                case "subject": record.subject ~= " " ~ continuation; break;
                case "date": record.date ~= " " ~ continuation; break;
                case "": break;
                default: assert(false);
            }
            return;
        }
        enforce(field(bytes), "mbox-index: malformed header at byte " ~ start_offset.toString);
        if (bytes.length >= 15) {
            auto name = bytes[0 .. 15];
            bool is_length = true;
            foreach (i, c; name) if (toLower(cast(char)c) != "content-length:"[i]) is_length = false;
            enforce(!is_length, "mbox-index: Content-Length framing requires a separate parser; stopped at byte " ~ start_offset.toString);
        }
        last_field = remember(record, bytes);
    }

    void body(scope const(ubyte)[] bytes, ulong start_offset, ulong end_offset) {
        if (postmark(bytes)) {
            pending_start = start_offset;
            pending_end = end_offset;
            state = State.pending;
        }
    }

    void acceptLine(scope const(ubyte)[] bytes, ulong start_offset, ulong end_offset) {
        final switch (state) {
            case State.first:
                enforce(postmark(bytes), "mbox-index: first line is not a recognizable mbox postmark");
                start(start_offset, end_offset, false);
                break;
            case State.headers:
                header(bytes, start_offset, end_offset);
                break;
            case State.body:
                body(bytes, start_offset, end_offset);
                break;
            case State.pending:
                if (field(bytes)) {
                    record.raw_end = pending_start;
                    emit(record);
                    start(pending_start, pending_end, true);
                    header(bytes, start_offset, end_offset);
                } else {
                    record.rejected_candidates++;
                    state = State.body;
                    body(bytes, start_offset, end_offset);
                }
                break;
        }
    }

    while (true) {
        auto read = source.rawRead(buffer);
        if (read.length == 0) break;
        foreach (c; read) {
            enforce(position != ulong.max, "mbox-index: byte offset overflow");
            enforce(line_length < max_line_length, "mbox-index: input line exceeds 1 MiB at byte " ~ line_start.toString);
            line[line_length++] = c;
            position++;
            if (c == '\n') {
                acceptLine(line[0 .. line_length], line_start, position);
                line_length = 0;
                line_start = position;
            }
        }
    }
    if (line_length != 0) {
        if (state == State.headers || state == State.first)
            enforce(false, "mbox-index: truncated postmark or header at end of file");
    }
    if (state == State.pending) record.rejected_candidates++;
    enforce(state != State.headers, "mbox-index: header block has no terminating blank line");
    if (state != State.first) {
        record.raw_end = position;
        emit(record);
    }
}

private import std.conv : to;
private string toString(ulong value) { return to!string(value); }
