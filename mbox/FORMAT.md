# mbox format notes

## Container first, message second

Treat the file as two nested formats:

1. **mbox container framing** — where messages start/end and how body lines were transformed to prevent false `From ` separators.
2. **Internet Message Format + MIME** — headers, addresses, dates, multipart bodies, transfer encodings, attachments.

Do not decode the whole mailbox to Unicode before finding byte boundaries. Historical mbox files may contain malformed, locale-dependent, 8-bit, or binary-looking data even though RFC 4155's portable `default` profile is stricter.

Preserve original byte offsets. A successful parse should be able to say which bytes belonged to each raw message before any unquoting, newline normalization, transfer decoding, or character decoding.

## RFC 4155 `default`

RFC 4155 defines a portable interchange profile, not a universal description of historical mailboxes.

Important properties of that profile:

- media type is `application/mbox`;
- messages are a linear sequence;
- every message is immediately preceded by a separator line;
- canonical line ending is one LF byte (`0x0a`), not CRLF;
- message data is seven-bit in the transferred canonical form;
- separator is `From ` + RFC-2822-style sender addr-spec + one space + UTC `ctime`-style timestamp + LF;
- every message is terminated by an empty line;
- RFC 4155 deliberately defines **no** body-line `From ` escape convention: a conforming reader is expected to recognize the complete separator syntax.

Compatibility parsing must therefore be a separate mode from strict RFC-4155 parsing.

## Historical dialects

| dialect | message-boundary basis | body `From ` handling | important loss/ambiguity |
| --- | --- | --- | --- |
| `mboxo` | scan for `From ` postmarks | escape bare `From ` as `>From ` | original `>From ` cannot always be distinguished from an escape produced by the writer |
| `mboxrd` | scan for `From ` postmarks | reversibly escape `From `, `>From `, `>>From `, ... by adding one `>` | safe only when the writer/reader agree on the reversible quoting convention |
| `mboxcl` | `From ` postmark plus `Content-Length` | historically mboxo-like quoting | byte-count semantics and producer behavior must be treated carefully; stale/corrupt lengths can desynchronize the file |
| `mboxcl2` | `Content-Length` determines body extent | no `>From` quoting | an mboxrd-style scanner can mistake unquoted body `From ` lines for new messages |

A single physical file may contain mixed conventions after concatenation or use by different programs. Dialect should therefore be recorded per parse/archive and, when evidence changes, potentially per message region.

## `From_` is envelope metadata, not the RFC `From:` header

The mbox separator is conventionally called the `From_` line to distinguish it from the message header `From:`. It carries envelope/postmark information. It is outside the embedded Internet message.

Do not silently replace one with the other. Keep at least:

- raw separator bytes;
- parsed envelope sender if recoverable;
- parsed delivery timestamp if recoverable;
- RFC message `From:` header separately.

Malformed separator metadata need not make the entire embedded message unreadable.

## Newlines

RFC 4155 canonical `default` requires LF. Files in the wild may contain CRLF or mixed line endings.

Parser rule:

- detect boundaries against bytes;
- preserve original newline bytes/spans;
- expose a normalized text view only above the raw/container layer;
- never let newline normalization invalidate a `Content-Length` byte count.

## Content-Length

For `mboxcl`/`mboxcl2`, `Content-Length` is a framing aid belonging to the mailbox representation. It must be parsed as a decimal byte count with overflow checks.

Do not trust it blindly. Validate that the computed end lands where a plausible next separator or EOF exists. On disagreement, keep the raw evidence and report a framing conflict rather than skipping arbitrary bytes without a diagnostic.

## Suggested parser modes

- `strict_rfc4155`
- `mboxrd`
- `mboxo`
- `mboxcl`
- `mboxcl2`
- `auto`

`auto` should be evidence-driven rather than a single permissive regex. It can score observations such as:

- strict RFC-4155-shaped separator;
- asctime-like historical postmark;
- consistent `Content-Length` fields;
- reversible `>From` quoting patterns;
- candidate separators at lengths predicted by `Content-Length`;
- conflicting candidate separators inside an otherwise valid message body.

Return the selected interpretation and diagnostics to the caller.

## Losslessness

There are two different products:

1. **raw archive view** — exact source bytes plus offsets;
2. **logical message view** — separator removed, dialect-specific quoting undone where justified, message/MIME parsed.

Never destroy the first in order to produce the second. Some malformed or mboxo-origin data is fundamentally ambiguous; the utility should be able to display that fact instead of inventing an original.

## First fixture matrix

At minimum:

1. empty mailbox;
2. one message, no final newline before mailbox-added termination;
3. two normal messages;
4. body line beginning `From `;
5. body line beginning `>From `;
6. body line beginning `>>From `;
7. RFC-4155 strict separator with LF;
8. CRLF mailbox;
9. mixed LF/CRLF;
10. `mboxcl` with correct length;
11. `mboxcl2` with unquoted body `From `;
12. too-short `Content-Length`;
13. too-long `Content-Length`;
14. overflowing/invalid length;
15. malformed postmark but valid embedded message;
16. concatenated regions using different dialects;
17. 8-bit header/body bytes;
18. MIME multipart whose body contains separator-looking text.

For every fixture, assert raw start/end offsets in addition to any decoded result.

## Security / robustness

The reader will consume untrusted archives. Treat all lengths and offsets as untrusted; use checked arithmetic; cap decoded expansion; avoid recursion proportional to attacker-controlled MIME nesting; do not execute attachments or HTML; and do not require a complete mailbox in memory.

A first version should be read-only. Writing/rewriting mbox is a separate project because choosing a dialect and rewriting quoting/lengths can irreversibly alter archives.
