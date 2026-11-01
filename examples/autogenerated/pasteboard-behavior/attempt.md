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

Tap-to-copy now attaches an advisory retained identity to its immutable ClipData
instead of preparing a marker in a separate history transaction. Source tests
exercise an intervening external capture and both mismatched and unknown hints.
The checked reducer alone decides whether the exact identity/text pair is an
own copy. Source/host checks, deterministic direct DEX and ICK/NDK compilation
pass locally. Private companion protocol v2 carries this hint; the stable user
service tag is retained, and candidate version code 5 requests replacement.
PB3 storage is unchanged. Installed loader, actual callbacks and physical-device
acceptance remain separate evidence obligations.

The integration heads were re-read during qualification. Idriç, ai-ci main,
Cat Food and android-NDK main remain at their initial inspected commits.
The canonical checked-export branch advanced from 035452331c0f2b98391a551e0797418da5793eae
to 6e1c9619a96f28494ad891a32a06edfb72f3e25b. Its code change saturates
optimized IO Unit command World closures; this application exports a pure
Text-to-Text reducer and does not use that command entry. The consumer keeps its
explicit 0354523 compiler pin while qualification proceeds, rather than
silently changing the executable boundary during a run.

At e0d9e7a110c4c536e5ecd0d7eb59f0b4b9379bfc, Actions run 41332839522,
API 29 and API 34 execute all 26 exact checked-reducer responses successfully
and reject the actual compiled ordering mutant at case 2 with exit 73. The
separate copy fixture now installs. A real version-2 APK upgrade preserves UID
and produces current/recovery records with exact C/B/A contents, independently
validated after download. The on-run migration observer stops before its
assertions because the artifact ZIP dropped its executable mode. The next run
restores the oracle from a mode-preserving inner archive and preflights it.
No migration assertion is weakened; privileged A/B/C and physical acceptance
are still unverified.

At 9b85f5bae3cdd7db5b76925a6046567ec08f9b09, run 41334889688,
the new clean-source check refuses the build because four declared dependency
checkouts appear as untracked directories: ai-ci, ICK host source, the public
test-signing checkout and tools. No application source changed. Those exact
dependency directories are now ignored at the consumer root; their identities
remain separately recorded. The clean-source assertion is retained. Feature
branch pushes no longer duplicate the existing pull-request qualification run.

At 819108c319477056d4490e937fded1ba3561fe1c / run 41336882942,
both ART jobs pass all 26 exact responses and reject the compiled mutant.
API 34 also passes migration, corrupt-legacy refusal, installed loader and
restart/replacement. The first automatic-mode UI action leaves its snapshot
unchanged (generation 1, capture enabled, automatic disabled), so the independent
mode assertion fails rather than claiming observation. Candidate version 6 adds
only lifecycle/touch/menu and foreground-read status diagnostics; no entry
identity or content is logged. UI screenshots are taken around the actual menu
action. API 29's independent run-as absence check is repaired separately using
the system ls executable with an explicit ENOENT requirement.

At c31afd5a660352ce40f616d82304436a7b706414 / run 41340603986,
independent migration passes on fresh API 29 and 34 environments. Both workflow
jobs execute all 26 ART responses and reject the compiled ordering mutant, then
fail the actual automatic-mode action. Native touch traces show the intended
menu row was invoked. Both Android SELinux audit logs deny `link` for the
private history backup and observer-key installation. This is a platform I/O
failure, not a touch-coordinate failure or a missing Shizuku authorization.

Candidate version 7 replaces history backup hard links with independently
written, synced files followed by atomic rename, and atomically renames a synced
private observer key. Host storage fault/crash/concurrency checks pass with the
new backup path. A failed mode write now preserves its error instead of trying
to enable an observer afterward. The companion process also follows the actual
APK native pointer width, so an ARM32 APK requests a 32-bit app_process. The
next Android run must re-establish every affected acceptance result; neither
these repairs nor the historical receipts establish privileged capture yet.

At 89dda5d07ab37621963ad8f507ebe9c7136d3b29 / run 41342118606,
API 29/34 migration still passes. API 34 now persists automatic mode, refuses
capture without Shizuku and after actual permission denial, then starts an
actual shell-owned companion after the real grant. A/B/C retention still fails;
the companion reconnects without successful heartbeats. Candidate 8 adds only
content-free initialization, listener, callback and delivery-stage diagnostics
to establish the failing SDK boundary. No clipboard content, capability or
exception message is logged. API 29 separately exposes delivery bookkeeping for
an empty text clipboard item on each foreground focus. The foreground adapter
now ignores that empty SDK item before constructing a delivery identity.

Explicit capture resumption prepares a freshly synced private credential while
the checked mode is still inactive. The companion discards entries from an old
credential and ignores late delivery failures from a replaced registration.
Ordinary process restart preserves the existing credential. Independent actual
Pause/copy/Resume and session-digest controls are added and remain unverified.
The exact pinned backend executable and TTC cache is bound to frontend/backend
source IDs and SHA-256 manifests. Local seed and verified reuse both compile the
current consumer, pass host controls and yield identical behavior/platform DEX.
This reuse does not skip consumer compilation, deterministic output comparisons,
source mutants, installed Android tests or current acceptance receipts.

At 882a4c2d8e2f8df1cbef09120499324e5d5394c8 / run 41343815598,
API 34 records actual companion UID 2000, app owner 10192, successful bridge
initialization and listener registration. Every heartbeat fails at the Context
resolver IPC with SecurityException, before a reply Bundle. No callback content
or successful history delivery is established. This matches Shizuku's explicit
user-service Context limitation. Candidate 9 replaces that invalid application
resolver boundary with the external-provider path in AOSP's own shell command,
using explicit API 29/34 framework signatures, balanced lifetime references and
the existing actual-UID/capability/mode/fresh-authorization gates. No global
hidden-API policy, keyboard or permission is changed to make it work.

The worker also verifies the shell package and operation-package identities,
checks bounded FD-free configuration Parcels and clears/restores only the
already authenticated incoming Binder identity while registering its public
clipboard listener as itself. These current sources compile/link through owned
ICK and NDK locally. Android observation and physical acceptance remain
unverified until fresh exact-head execution; the historical registration
success does not establish content delivery for these changes.

At d50b9917d9296ec97f0366697b341536b46a2905 / run 41345425096,
both APIs pass migration, installed restart/replacement and all exact ART/mutant
controls. Both decisive capture workflows still fail. API 34's new explicit
shell identity check refuses initialization before bridge setup. AOSP's exact
API 29/34 ContextImpl constructor carries its container's operation-package
identity through createPackageContext. Candidate 10 rebuilds a genuine shell
LoadedApk context through the bounded createAppContext factory and verifies its
UID, package and operation-package identities before public SDK registration.
Fresh Android receipts must establish callback read and private writer delivery.

A separate real-fork crash test proves that a pre-replacement pending snapshot
survives a later acknowledged deletion in the prior store. The new checkpoint
retires only owned, regular 0600 files matching the actual pending filename
protocol, under the exclusive writer lock after both committed copies are
synced. Directory cleanup is synced before success. The regression now passes,
including preservation of an unrelated similarly named file, crash recovery,
full-storage/error controls and concurrent transactions. The independent Android
UI test also seeds an abandoned snapshot from already captured SDK history and
requires confirmed deletion to retire it. These are filesystem controls, not
synthetic capture evidence.
