# Fixed Pasteboard

The retained NativeActivity shows three unlabeled spatial boards, starting in
the middle. Each portrait board has sixteen rounded positions in two columns
and eight rows. Tap a card to copy its exact text. Swipe a card horizontally to
move it to the adjacent board; swipe header or unused space to change boards.
Three dots show the selected board. Each occupied card has a visible menu for
protection and confirmed deletion. The header menu exposes capture mode,
pause/resume, archive access, archive clearing and Shizuku setup.

This is an **unqualified candidate**, continued in
[utilities-android-phone-user PR #90 — Recover fixed Pasteboard on current main and reuse checked UTF-8 clipboard JNI](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/90),
tracking [issue #89 — Finish Fixed Pasteboard](https://github.com/Ashtray-Archer/utilities-android-phone-user/issues/89).
Keep the PR in draft until the complete acceptance contract is satisfied.

## Source ownership

`idric/Pasteboard.idric` retains the original layout contract. The build checks
its generated header and its three-board, sixteen-slot assertions.
`idric/PasteboardBehavior.idric` owns capture, exact-text deduplication, ordering,
movement, selection, protection, deletion, visible eviction, archived retention,
capture mode and checked snapshot decoding. Its `Text → Text` export compiles
through the pinned current Idriç frontend and canonical android-NDK DEX writer.

The native history adapter frames requests, invokes that actual export through
JNI, projects the returned state for rendering, and commits opaque snapshots.
It contains no second application model. The old bounded C model survives only
under `app/src/test-c/legacy-v1` as an unchanged v1 migration fixture.

The canonical NativeActivity packager receives the typed platform-declaration
DEX as `classes.dex`. The checked reducer DEX is an immutable packaged asset,
`assets/pasteboard-behavior.dex`, loaded by the public API-26
`InMemoryDexClassLoader` and executed by ART. Both inputs and the APK are
digest-bound. A successful source compile or DEX decode is not installed-loader
or Android execution evidence. No Java/Kotlin source, javac, d8, Gradle, RefC,
native interpreter, arbitrary DEX patching or generated replacement app is used.

## History policy

New text enters the selected board first. An exact-text duplicate is globally
unique; recopying an older record promotes the same record on its current board
and preserves protection. Repeating the immediately seen value leaves order
unchanged. A card copy carries an advisory retained identity in the same
immutable Android ClipData. The checked reducer verifies the identity and exact
text before preserving its position, including when another capture intervenes.
The hint grants no authority and cannot suppress different or new text. A
failed clipboard write leaves history untouched.

When a board is full, its oldest unprotected card moves to the archive. Its full
text remains retained. If all sixteen cards are protected, a new capture goes
directly to the archive; a move into that board is refused without modifying the
source. Deletion requires confirmation and refuses protected records. Archive
clearing removes only unprotected archived records. There is no automatic
archive expiry; the user controls deletion and capture pause.

Single plain-text clipboard items preserve significant whitespace, CR/LF,
indentation, fences and supplementary Unicode. Previews may collapse whitespace
and ellipsize; stored and copied bytes are unchanged. Embedded NUL is explicitly
refused, never truncated. Non-text and multiple-item clipboard values are not
captured. Sensitive-marked values are skipped. No history content is logged,
uploaded or sent over a network. Android clipboard expiration does not expire
previously retained history.

Private `history.v2` records have a versioned 40-byte length/checksum header and
length-delimited UTF-8 payload. An independently opened file lock serializes
observer and UI updates. Writes use a private synced sibling and atomic rename,
with a validated previous generation for recovery. Corrupt or unreadable history
fails closed rather than becoming an empty history. Full storage and uncertain
final directory sync remain explicit errors. A deletion or archive clear also
replaces the recovery image with an independently synced copy of the checked
result before acknowledgement. Its version-3 marker survives an interrupted
checkpoint; the next validated read must finish that checkpoint before showing
success. Deletion also clears a matching raw seen marker. Older version-2
records remain readable. Current host tests cover both formats, interrupted
checkpoints and recovery without resurrecting acknowledged deleted text.

Valid original `pasteboard.bin` v1 records are imported through the checked
reducer when no new-format history exists. The original file remains until two
independently synced modern copies exist, then is retired. An incompatible or
damaged legacy record stops migration and preserves the original. The stable
package is `com.isomorphisms.pasteboard`; candidate version code 8
uses the persistent Wegert public test certificate
`de9b1d47c5a65e6d46a204b79dd9ee566b9d3c9832ba81ebc4213d3392e92ff9`.
An installation with another legacy signer cannot be replaced by Android;
preserve/export its data before any explicit uninstall. The build never
uninstalls to bypass a signer mismatch.

## Automatic capture candidate

The user explicitly selects automatic mode and authorizes Pasteboard in
Shizuku. Gboard remains the selected keyboard. A Shizuku API-13 daemon user
service requests public `ClipboardManager` callbacks as UID 2000 with package
identity `com.android.shell`. The private clipboard payload never goes through
Shizuku manager RPC. Callback text enters a FIFO; SDK `ContentProvider.call`
delivers it to the application's private history writer. An acknowledgement
requires a synced checked-state commit. Durable delivery identities prevent
reordering on an acknowledgement retry.

The provider requires Android's cross-user permission, actual shell caller UID,
a private random capability, selected automatic mode and fresh Shizuku
authorization. Paused, foreground-only and revoked states reject delivery.
Starting a new session after Pause or foreground-only mode atomically replaces
the private capability before checked capture is re-enabled. The companion
discards queued events authenticated for an earlier session. Ordinary process
restarts keep the current credential. Heartbeats expose connection loss and
permit authorized reconnection. Pending unacknowledged events from the current
session remain in companion memory; a companion crash can lose
that pending queue and clipboard values copied during downtime. History already
acknowledged remains local and durable. Reconnection cannot reconstruct missed
system clipboard history.

The manager attachment and user-service Binder protocols are bounded to API 13
and the pinned Shizuku 13.6 source; platform callbacks and clipboard access use
public SDK methods. The actual shell Context, clipboard listener delivery,
permissions, companion lifetime, revocation and reconnection all require fresh
API 29/34 and physical-device qualification. Until that evidence exists this
route must not be described as working automatic history. Foreground-only mode
captures the current value on focus and cannot recover intermediate copies.

## Build and acceptance

Use `build-checked.grease` with explicit absolute repository, frontend, backend
and fresh output paths. It checks the original layout, runs current model tests,
compiles direct DEX twice for byte determinism, independently decodes it, and
compiles a wrong-order source mutant which must fail the ordering oracle.
`build-apk.grease` consumes those exact outputs, qualified ABI-specific ICK
stages and NDK 27.2.12479018, then calls the canonical Android APK producer.
It emits separate ARM64, ARM32 and x86_64 candidates with signer and packaging
receipts. The former DEX-free `build-apk.sh`/`check-apk.sh` entry points are
retired because they cannot represent the completed checked-model architecture.

The workflow retains source/build receipts and candidates, then separately
tests installed loader initialization, process restart and replacement on API
29 and 34. These are not the decisive cross-application test. Independent ai-ci
qualification, actual A/B/C copies in another application, long text, observer
negative controls, complete UI interaction and physical C67/TAB_P10 acceptance
remain separately reported obligations. No emulator result counts as a physical
test. No partially qualified APK is a finished release.
