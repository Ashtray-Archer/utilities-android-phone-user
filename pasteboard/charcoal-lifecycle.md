# Retain the drawing producer for its native-window lifetime

The actual signed charcoal APK at e3b018bfa12cd1d33cfd8ee4934ef3ce688c2a66
passes its existing suites and the API34 five-app lifecycle replay. On API29,
run41976766131 retains a real Home/resume crash: Surface.lockCanvas reconnects
an already connected CPU producer, throws IllegalArgumentException and kills
the native thread. The failed artifact13391369878 contains the full framework
stack, window state, screenshots and startup video.

The renderer previously converted the native window to a new Java Surface for
each frame and only deleted the local JNI reference. The platform retains that
native reference until Java collection. Cache one global Surface for the active
native window, borrow a local reference for each unchanged Canvas session, and
explicitly release it on TERM_WINDOW or destruction. This also avoids repeated
wrapper creation and does not add a render loop or asynchronous worker.

Clear lockCanvas exceptions even when its result is null; boolean short-circuit
evaluation previously bypassed that cleanup and let the pending exception escape
when detaching the native thread. A failed lock remains a failed draw, never a
successful visual receipt.

Package, permanent test signer, stored history, controls, portrait lock and theme
remain unchanged. The existing producer and fresh API29/API34 Home/resume replay
must qualify this repair; the old API29 failure is not relabeled as passed.

Platform lifetime contract:
https://developer.android.com/ndk/reference/group/native-activity#anativewindow_tosurface

The signed29d6a7a candidate now passes both API29/API34 system modes, rotation
policy, Home/resume, force-stop reopen and all-frame broad-white startup guard
in ai-ci41980041161. Its separate full workflow API35 fails post-reboot launch;
API34 is cancelled after a long wait in that region. The old observer can
accept the previous kernel's boot-complete value. Pin the new-kernel observer
for both fixture preparation and execution, retaining the separate unchanged
ICK producer. The preceding observer5ad01a4 passed real reboots on all three
APIs; the bounded current source must requalify. Require exact saved history
bytes and never reinstall or recreate state after reboot. Fresh full-suite
qualification must establish whether this resolves the captured failure.
