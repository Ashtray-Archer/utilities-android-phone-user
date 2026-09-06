# mbox sources

Source-first inventory. Each item is a document or source with its own provenance and project-facing note. The summaries below are derivative notes; they are not substitutes for the originals.

## 1. RFC 4155 — The application/mbox Media Type

- Author: Eric A. Hall
- Date: September 2005
- Status: Informational RFC
- Canonical: https://www.rfc-editor.org/rfc/rfc4155.html
- Plain text: https://www.rfc-editor.org/rfc/rfc4155.txt
- IETF Datatracker: https://datatracker.ietf.org/doc/rfc4155/
- Local mirror: fetched by `_/sources/fetch.sh` as `rfc4155.txt`

This is the main standards document. It registers `application/mbox` and defines a portable `format=default`. It is unusually explicit that historical mbox is not one authoritative format. Appendix A is therefore a canonical interchange profile, not a description of everything found in old mailboxes.

Project use: keep RFC-4155-default conformance separate from compatibility parsing for historical dialects.

## 2. qmail `mbox(5)` — file containing mail messages

- Author/source lineage: D. J. Bernstein / qmail
- Historical URL cited by RFC 4155: http://qmail.org/man/man5/mbox.html
- Maintained source mirror used here: https://github.com/notqmail/notqmail/blob/master/mbox.5
- Local mirror: [`_/sources/qmail-mbox.5`](%5F/sources/qmail-mbox.5)

RFC 4155 itself points readers at this document as a mostly authoritative description of Unix mbox variations. It gives concrete read/write algorithms for `mboxrd` and names `mboxo`, `mboxcl`, and `mboxcl2`.

Project use: this is the primary compatibility reference for `From ` quoting and the four named dialects.

## 3. Rahul Dhesi proposal / historical `mboxrd` discussion

- Original proposal date: 24 June 1995
- Reposted by Rahul Dhesi in a 1996 thread: https://groups.google.com/g/comp.mail.headers/c/Q6GXtTBBJys/m/J3SVqCTVdRwJ
- Local mirror: link only

The historical proposal explains why reversible quoting is needed: an original body line beginning `>From ` must remain distinguishable from a line escaped by the mailbox writer.

Project use: useful for tests that prove quote/unquote round trips rather than merely recognizing delimiters.

## 4. RFC 5322 — Internet Message Format

- Canonical: https://www.rfc-editor.org/rfc/rfc5322.html
- Plain text: https://www.rfc-editor.org/rfc/rfc5322.txt
- Local mirror: fetched by `_/sources/fetch.sh` as `rfc5322.txt`

RFC 4155 cites RFC 2822; RFC 5322 supersedes it for the message syntax inside the mbox container.

Project use: after mbox splitting, parse headers and message structure against IMF rules. Container parsing and message parsing remain separate stages.

## 5. RFC 2045 — MIME Part One

- Canonical: https://www.rfc-editor.org/rfc/rfc2045.html
- Plain text: https://www.rfc-editor.org/rfc/rfc2045.txt
- Local mirror: fetched by `_/sources/fetch.sh` as `rfc2045.txt`

Defines MIME-Version, Content-Type, Content-Transfer-Encoding, and related body representation rules.

Project use: needed only after message boundaries and headers are trustworthy; do not make MIME decoding part of the first splitter.

## 6. RFC 2046 — MIME Part Two: Media Types

- Canonical: https://www.rfc-editor.org/rfc/rfc2046.html
- Plain text: https://www.rfc-editor.org/rfc/rfc2046.txt
- Local mirror: fetched by `_/sources/fetch.sh` as `rfc2046.txt`

Defines multipart media types and boundary processing. RFC 4155 also points to `multipart/digest` as a more strictly specified alternative for exchanging message collections.

Project use: attachments and multipart bodies are a later layer than mbox framing.

## 7. IANA media-type registries

- Media types: https://www.iana.org/assignments/media-types/media-types.xhtml
- Registry index: https://www.iana.org/protocols
- Local mirror: link only

IANA records `application/mbox` with RFC 4155 as its reference and maintains the media-type sub-parameter registry described there.

Project use: MIME-type recognition should accept `application/mbox`; an unknown declared mbox `format` parameter must not silently be treated as a known dialect.

## 8. Mutt / NeoMutt `mbox(5)`

- Mutt manual: https://mutt.org/doc/manual/
- NeoMutt man page: https://neomutt.org/man/mbox
- Debian-rendered man page: https://manpages.debian.org/trixie/mutt/mbox.5.en.html
- Local mirror: link only

Modern implementation documentation describes the line-oriented mailbox, `From_` postmark, and the `MBOXO` versus `MBOXRD` quoting schemes.

Project use: independent implementation cross-check against the qmail description; useful for compatibility fixtures.

## 9. Dovecot mbox documentation

- Documentation: https://doc.dovecot.org/2.3/admin_manual/mailbox_formats/mbox/
- Historical text source: https://sources.debian.org/src/dovecot/1%3A2.2.27-3~bpo8%2B1/doc/wiki/MailboxFormat.mbox.txt/
- Local mirror: link only

Dovecot documents the named variants and also demonstrates an important real-world point: a tool may use `Content-Length` to avoid relying on `From ` quoting while still calling the mailbox mbox.

Project use: test mixed/implementation-specific files and do not infer a dialect from a single superficial feature.

## 10. Library of Congress format descriptions

- MBOX family: https://www.loc.gov/preservation/digital/formats/fdd/fdd000383.shtml
- MBOXO: https://www.loc.gov/preservation/digital/formats/fdd/fdd000384.shtml
- MBOXRD: https://www.loc.gov/preservation/digital/formats/fdd/fdd000385.shtml
- MBOXCL: https://www.loc.gov/preservation/digital/formats/fdd/fdd000386.shtml
- MBOXCL2: https://www.loc.gov/preservation/digital/formats/fdd/fdd000387.shtml
- Local mirror: link only

These are useful secondary descriptions of interoperability and preservation hazards across the four named variants.

Project use: source for adversarial compatibility cases and for documenting when exact recovery is impossible.

## Mirroring policy

Raw third-party text goes under `_/sources/` with provenance. Canonical URLs stay in this file even when a local mirror exists. `fetch.sh` retrieves RFC text from the RFC Editor so a checkout can reconstruct the standards corpus without burying large reference documents in the useful project surface.
