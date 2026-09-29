module mbox.scanner_tests;

import mbox.scanner : MessageRecord, scan;
import std.stdio : File;
import std.string : indexOf;

private MessageRecord[] readFixture(string bytes) {
    auto file = File.tmpfile();
    file.rawWrite(cast(const(ubyte)[]) bytes);
    file.seek(0);
    MessageRecord[] records;
    scan(file, (in MessageRecord record) { records ~= record; });
    return records;
}

private bool rejects(string bytes, string diagnostic) {
    try {
        readFixture(bytes);
    } catch (Exception error) {
        return error.msg.indexOf(diagnostic) >= 0;
    }
    return false;
}

unittest {
    assert(readFixture("").length == 0);
    assert(rejects("Subject: no separator\n\n", "first line"));

    enum first = "From alice@example.com Sat Jan  1 00:00:00 2022\n";
    enum second = "From bob@example.com Sun Jan  2 00:00:00 2022\n";
    enum text = first ~
        "From: Alice <alice@example.com>\nTo: Bob <bob@example.com>\n" ~
        "Subject: First\n continuation\nDate: Sat, 1 Jan 2022 00:00:00 +0000\n\n" ~
        "body\n>From escaped\n>>From also escaped\n" ~ second ~
        "From: Bob <bob@example.com>\nSubject: Second\n\nlast without newline";
    auto records = readFixture(text);
    assert(records.length == 2);
    assert(records[0].raw_start == 0);
    assert(records[0].raw_end == text.indexOf(second));
    assert(records[0].separator_end == first.length);
    assert(records[0].header_end == text.indexOf("\n\nbody") + 1);
    assert(records[0].body_start == text.indexOf("body\n"));
    assert(records[0].sender == "Alice <alice@example.com>");
    assert(records[0].recipients == "Bob <bob@example.com>");
    assert(records[0].subject == "First continuation");
    assert(records[0].date == "Sat, 1 Jan 2022 00:00:00 +0000");
    assert(!records[0].possible_boundary);
    assert(records[1].raw_start == text.indexOf(second));
    assert(records[1].raw_end == text.length);
    assert(records[1].possible_boundary);
    assert(records[1].subject == "Second");

    enum fake = "From plausible@example.com Tue Jan  3 00:00:00 2022\n";
    auto with_fake = readFixture(first ~ "Subject: one\n\nbody\n" ~ fake ~ "not a header\nend\n");
    assert(with_fake.length == 1);
    assert(with_fake[0].rejected_candidates == 1);
    auto at_end = readFixture(first ~ "Subject: one\n\nbody\n" ~ fake);
    assert(at_end.length == 1);
    assert(at_end[0].rejected_candidates == 1);

    // Cross a 64 KiB read boundary without retaining the body in memory.
    auto long_body = new char[128 * 1024];
    long_body[] = 'x';
    auto long_text = first ~ "Subject: one\n\n" ~ long_body.idup ~ "\n" ~ second ~
        "Subject: two\n\nend\n";
    auto long_records = readFixture(long_text);
    assert(long_records.length == 2);
    assert(long_records[0].raw_end == long_text.indexOf(second));
    assert(long_records[1].raw_end == long_text.length);

    enum windowsText = "From a@example.com Sat Jan  1 00:00:00 2022\r\n" ~
        "From: A\r\nSubject: Two\r\n\r\nBody\r\n";
    auto windows = readFixture(windowsText);
    assert(windows.length == 1);
    assert(windows[0].body_start > windows[0].header_end);
    assert(windows[0].raw_end == windowsText.length);

    assert(rejects(first ~ "Content-Length: 12\n\nFrom danger\n", "Content-Length"));
    assert(rejects(first ~ "Subject: missing blank\n", "no terminating blank"));
    assert(rejects(first ~ "malformed header\n\n", "malformed header"));
    auto overlong = new char[1024 * 1024];
    overlong[] = 'x';
    assert(rejects(first ~ "Subject: one\n\n" ~ overlong.idup ~ "\n", "line exceeds 1 MiB"));
}

void main() {}
