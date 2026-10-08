# Remaining G3 acceptance

Inputs: source `abe5bd2d6b54dcccf4f2badb84457dca63e35973`, the separately
identified A1/C67 candidate APKs and [receipts](receipts/2026-10-08/README.md).
Current state: both builds/packages PASS; normal producer approval BLOCKED on
package registration; all physical actions below NOT_RUN. No device session is
assumed. This continues G3 and does not introduce another architecture job.

First obtain normal merged-main producer approval for each exact APK and retain
its receipt. Signer registration is prepared in
[isomorphisms/ai-ci PR #229, “Register Green Grocer's stable public test Android signer”](https://github.com/isomorphisms/ai-ci/pull/229).
No release, merge, uninstall, device compilation or toolchain bootstrap is
authorized by this record. Use the established device delivery mechanism when
available; this document does not reopen deferred ADB/setup work.

On each physical target separately, record device identity, window size/density,
ABI, exact APK digest, package/version/signer and these actual results:

1. Install, launch, and observe empty session with sample total $0.00.
2. Add bananas twice: amount 2, total $2.58. Decrement once, add apples twice
   and cereal three times: three selected products and total $22.64.
3. Switch to selected-only and back. Remove apples: total $12.66. Remove all
   remaining selections: total $0.00 and an empty selected-only view. Return
   to all eight products; remove an already absent product without changing total.
4. Scroll from a quantity control with a drag exceeding tap slop: scrolling
   must not change the cart. A normal tap changes the amount once. Drag away
   and back, cancellation and a second pointer must not commit an accidental tap.
5. Check readable wrapped names/units and touch controls at the actual window
   size, including the bottom rows. Check header/footer overlap and rotation
   or an available narrow/wide window configuration.
6. Background/resume and recreate the surface without killing the process:
   cart persists and controls remain correct. End the process and relaunch:
   cart starts empty. Observe idle operation without a continuous redraw loop.
7. Replace the installed package using the same persistent signer and a
   nondecreasing version code, without uninstalling. Record replacement and
   subsequent launch/interaction separately. A signer/downgrade failure stays
   a failure and must not be worked around by uninstalling.

Acceptance requires the intended results on A1 and C67, with failures preserved
before bounded repairs. Package inspection, host tests, source reasoning and
target detection do not count as these physical results. Android execution of
the standalone core tests remains a separately labeled unrun coverage boundary.
Exclude inventory, Eagle, persistence, accounts, payment and framework expansion.
