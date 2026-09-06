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
