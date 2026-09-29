# mbox

Phone utility for reading mbox mail archives.

The repository work starts from the format sources rather than from a library API. Each source document gets its own entry in [`SOURCES.md`](SOURCES.md); raw mirrors and reproducible fetches live under [`_/sources`](%5F/sources). Project-facing synthesis comes after the source inventory.

## Useful files

- [`SOURCES.md`](SOURCES.md) — one entry per specification, historical source, or implementation reference; canonical links and local-mirror status.
- [`FORMAT.md`](FORMAT.md) — mbox container model, dialect differences, parser invariants, and failure cases.
- [`DEX-JNI.md`](DEX-JNI.md) — plan for lowering the phone utility to DEX plus a narrow JNI/native boundary.
- [`_/sources`](%5F/sources) — raw mirrored material and a fetch script. This is reference/build material rather than the user-facing project surface.

## First project slice

1. Accept an mbox file as bytes/stream; do not assume the `.mbox` suffix proves the format.
2. Split it into messages without destroying source bytes.
3. Record which delimiter/quoting interpretation was used and any ambiguity or damage.
4. Parse enough Internet Message Format headers to list sender, recipients, subject, and date.
5. Decode text bodies/MIME only after container splitting is correct.
6. On Android, keep framework/file-picker/UI work on the DEX side and keep byte-oriented parsing behind a small JNI/native ABI.

The parser must treat `mbox` as a family of incompatible conventions, not as one grammar with one trustworthy delimiter rule.

## First working command: `mbox-index`

`mbox_index.d` and `scanner.d` provide a read-only D command for a host machine.
The DUB build definition is `dub.sdl`. `mbox-index MAILBOX` streams a TSV index
to standard output with half-open byte ranges (`raw_start` through `raw_end`),
separator and header ends, body starts, selected header previews, and boundary
diagnostics. It needs no network access and does not modify the mailbox.

This first mode recognizes postmark-shaped `From ` lines, checks that the next
line starts a header block, and records later boundaries as `possible`. A body
can still contain both a plausible postmark and a header-shaped next line; do
not treat those boundaries as proven. It reports rejected postmark candidates
inside a message. The `dialect` column says `postmark/quoting-unknown` because
this slice does not unquote `mboxo`/`mboxrd` bodies or infer a file-wide dialect.
It rejects `Content-Length` headers instead of misreading `mboxcl`/`mboxcl2`.
Malformed headers, header blocks over 64 KiB, and lines over 1 MiB also stop
the scan with a nonzero result. Header previews are ASCII only; high-byte and
control characters become spaces. The raw archive remains untouched, and MIME
decoding comes later.

The scanner's `scan` entry point accepts an already open file and emits records
as they are completed. The CLI prints them immediately, so an error late in a
file can leave partial TSV on stdout; consumers must check the exit status.
The D unit checks in `scanner_tests.d` exercise byte ranges, folded headers,
CRLF input, escaped and false postmarks, unsupported framing, and a body that
crosses read buffers. These are host checks. The Android DEX/JNI path in
`DEX-JNI.md` has not been built or run.
