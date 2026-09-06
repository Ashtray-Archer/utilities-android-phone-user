# raw sources

Reference material kept out of the useful project surface.

## Mirrored now

### `qmail-mbox.5`

- upstream lineage: qmail `mbox(5)`
- source used for this copy: `notqmail/notqmail`, commit `b2b7a1fe63b340d42ff1fbe8f6d77cfa21e74622`
- source URL: https://github.com/notqmail/notqmail/blob/b2b7a1fe63b340d42ff1fbe8f6d77cfa21e74622/mbox.5
- historical URL cited by RFC 4155: http://qmail.org/man/man5/mbox.html
- mirrored: 2026-09-06

Keep the file byte-for-byte/source-text oriented; put commentary in `../../SOURCES.md` or `../../FORMAT.md`, not into the mirror.

## Reconstructable RFC mirrors

Run:

```sh
./fetch.sh
```

It downloads the plain-text RFC Editor copies of:

- RFC 4155 — application/mbox;
- RFC 5322 — Internet Message Format;
- RFC 2045 — MIME Part One;
- RFC 2046 — MIME Part Two.

Those fetched files are reference inputs. The stable provenance and project notes live in `../../SOURCES.md`.
