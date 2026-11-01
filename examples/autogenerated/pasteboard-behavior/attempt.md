# Fixed Pasteboard executable model

## Intended boundary

Continue the recovered application on `recover-pasteboard-20261101`. The checked
Idriç reducer owns capture, exact-text duplicate handling, ordering, board
selection, movement, pinning, deletion and archive retention. C owns Android
clipboard conversion, rendering, JNI invocation, file locking and atomic byte
storage. It must not implement a second reducer.

Values are decoded `Text`, entry identities, three board positions, protected
entries, a sequence of retained entries, and explicit results. The model keeps
all retained text separately from the sixteen visible positions on each board.
Storage encoding and Android classes are boundary representations.

The central exported operation is a checked `Text → Text` transition. Its input
contains one length-delimited action and a complete length-delimited snapshot;
its result contains an explicit outcome and the next validated snapshot.
Lengths count decoded Unicode scalar values in the reducer protocol. The file
adapter separately frames standard UTF-8 by byte length and validates integrity.
No character count is reused as a byte count.

## Invariants and decisions

- Initially select the middle of three unlabeled boards, each with 16 visible
  positions, two columns and eight rows.
- A capture inserts newest first. Exact duplicate text has one retained entry;
  recopying promotes that entry without discarding its protection or board.
- Tapping an entry marks that clipboard value seen, so the app's own copy does
  not become a new capture.
- Overflow archives the oldest unprotected visible entry. An entirely protected
  board sends a new capture directly to the archive. No overflow deletes text.
- Only adjacent-board movements are allowed. A full protected destination
  refuses a move and leaves the source intact.
- Protection blocks deletion until explicitly removed. Deletion is explicit and
  must be committed atomically before the UI reports success.
- Embedded NUL is refused with a visible unsupported-text result. Empty and
  non-text clipboard items are distinguished from unavailable clipboard access.
- Sensitive clips are excluded from durable capture by default. Clipboard
  contents must never appear in logs or network traffic.

## Required execution

Use the canonical direct DEX backend and checked source frontend. Qualify a
separate caller against emitted DEX under ART; compile semantic source mutants
and require the same observer to reject them. Host reference behavior and DEX
decoding remain separate evidence. First-party Java/Kotlin, generated Java,
`javac`, `d8`, Gradle and RefC are excluded.

Starting inspected heads:

| Repository | Revision |
| --- | --- |
| utilities recovery | `18411b63faf7fa50a935bb04e38ff5669fcf1a47` |
| utilities main | `34b2391a97867ab25ea885cedc7ac2e04b591609` |
| android-NDK main | `6ab4818219efa7ea5eda137ca6b76976eb2a4d42` |
| Idriç | `0e8cfdd1106ebdeffd01b6c519880ea3afc7dc35` |
| ai-ci main | `a66e3a9bb1070a338f8c4a67f9b4b4ac39dfb5ec` |
| Cat Food main | `eafd7fa9e61138a664b205bf8ba82ec6837bbbb9` |

## Execution record

The current checked source passes its host behavior checks. Direct DEX emission,
independent baksmali decoding and repeated-byte determinism pass. An actual
compiled wrong-order source mutant is rejected by the ordering oracle. These
are not ART or installed-APK acceptance.

The platform declarations use the canonical typed framework encoder. The
canonical NativeActivity producer accepts one declared DEX input, so JNI
platform classes occupy `classes.dex` and the checked reducer remains an
immutable DEX asset loaded by API-26 `InMemoryDexClassLoader`. This is ART code
execution, not a native interpreter. Both DEX inputs must be bound to installed
acceptance receipts; execution is still pending.

The first integrated exact-head build (`49408003ae67dd34102eb41fddf1f84cddd3c0a7`)
typechecked/emitted DEX and passed the host controls, then correctly refused
file opening under the existing Fortify profile.
[ai-ci PR #333 — Qualify checked file opening and independent Fixed Pasteboard acceptance](https://github.com/isomorphisms/ai-ci/pull/333) extends that
shared profile with checked open/openat varargs and independent negative
controls rather than disabling hardening. The extension passes actual Bionic
static runtime at O1/O2/O3/Os on the Linux x86_64 host; ARM ABI and Android tests
remain separate.

Local ICK-to-NDK x86_64 linking then exposed unsupported GNU TLS symbol
`__tls_get_addr` at the API-26 boundary. Native per-thread diagnostic buffers now
use public pthread keys and destructors. No application semantics moved back
to C. That current x86_64 native library compiles and links successfully.

The first Grease parse rejected a quoted glob in a word list; the coordinator
uses the documented `io.glob` expression instead and has executed successfully
through the exact Grease source launcher. No alternate shell implementation was
used. Installed DEX/ART, observer, full Android UI and physical-device acceptance
remain pending.

Full current requalification correctly rejected the original retained ICK stage:
the old workflow selected `c61e448251744a2f40ad743ebef1a027bdcd2f9d`, not the
assumed current `fbe86e23d55cfec2000c08e61deea2a407fd7175`. Its actual x86_64
complex extraction failure is retained in ai-ci's
`examples/autogenerated/fixed-pasteboard/retained-source-mismatch.md`. Current
builds restore freshly qualified stages, check their source receipt and execute
the current profile again. The mislabeled old frontend cache is invalidated.

Deletion now removes a matching raw seen value and requires an independently
synced current recovery image before acknowledgement. A version-3 marker makes
an interrupted checkpoint resumable after checked model validation. Host tests
pass actual process death at that boundary, no-space refusal, separate recovery
inodes and recovery that does not resurrect acknowledged deleted text. Both
modern formats remain supported. The original v1 file is retired only after
two synced modern copies exist. These current changes also compile/link through
ICK and NDK locally; installed acceptance remains separate.
