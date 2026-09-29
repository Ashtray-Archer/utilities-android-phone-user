module mbox.mbox_index;

import mbox.scanner : MessageRecord, scan;
import std.stdio : File, stderr, stdout;

private void cell(string value) {
    foreach (c; value) stdout.write(c == '\t' || c == '\n' || c == '\r' ? ' ' : c);
}

int main(string[] args) {
    if (args.length == 2 && (args[1] == "--help" || args[1] == "-h")) {
        stdout.writeln("usage: mbox-index MAILBOX");
        stdout.writeln("read-only postmark scan; Content-Length archives are rejected");
        return 0;
    }
    if (args.length != 2) {
        stderr.writeln("usage: mbox-index MAILBOX");
        stderr.writeln("read-only postmark scan; Content-Length archives are rejected");
        return 2;
    }
    try {
        auto source = File(args[1], "rb");
        stdout.writeln("number\traw_start\traw_end\tseparator_end\theader_end\tbody_start\tdialect\tboundary\trejected_candidates\tfrom\tto\tsubject\tdate");
        size_t number;
        scan(source, (in MessageRecord record) {
            stdout.write(++number, '\t', record.raw_start, '\t', record.raw_end, '\t',
                record.separator_end, '\t', record.header_end, '\t', record.body_start,
                "\tpostmark/quoting-unknown\t",
                record.possible_boundary ? "possible" : "first", '\t', record.rejected_candidates, '\t');
            cell(record.sender); stdout.write('\t');
            cell(record.recipients); stdout.write('\t');
            cell(record.subject); stdout.write('\t');
            cell(record.date); stdout.writeln();
        });
        return 0;
    } catch (Exception error) {
        stderr.writeln("mbox-index: ", error.msg);
        return 1;
    }
}
