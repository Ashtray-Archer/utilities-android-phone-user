# Pasteboard

A small Android pasteboard built as a native phone utility, with the board shape owned by Idriç and the current Android runtime boundary implemented in C/JNI.

## First useful version

- three spatially distinct pasteboards;
- boards are **unlabeled by default** and begin on the middle board;
- 16 slots per board, arranged as two columns × eight rows in portrait mode;
- newest captured text enters slot 1 and older entries trickle forward through the 16 slots;
- all 16 positions remain visible as rounded cards, including empty slots;
- tap a card to copy it back to Android's clipboard;
- swipe a card left or right to move it to the neighboring board, where it enters at slot 1 and the destination board trickles down;
- swipe the empty/header area horizontally to change boards;
- the current board is shown only by three small position dots, avoiding permanent labels and toolbar chrome;
- short text is drawn larger; longer text is reduced and visibly ellipsized rather than shrinking to unreadable type;
- state persists in the app's private storage.

There is deliberately no delete, trash, pin, search, or board-label editor in this slice. Those can be added after the basic spatial behavior is useful on the phone.

## User story: spatial boards without prescribed meanings

As a phone user, I want a few pasteboards that I can learn by position rather than having the app permanently name them for me. I may decide that one board is for school and the kids, where a standard parent/signature text should remain easy to reach; I may use another for mathematical characters or programming fragments; or I may use all three for something completely different. The product should preserve that flexibility instead of spending screen space on labels I may not need. Optional user-supplied labels can be considered later without making labels part of the basic model.

## Clipboard boundary

A normal Android application cannot retrospectively read the operating system or Gboard clipboard history, and modern Android restricts background clipboard reads. This app therefore owns history only after it has seen an item. In this first version it reads the current clipboard when the app gains focus and stores new clipboard text into the currently visible board. It cannot recover everything pasted before installation, nor every intermediate clipboard value copied while the app remained in the background.

The stored pasteboard itself is persistent: entries remain across app restarts until they are displaced by newer entries or later deletion functionality is implemented.

## Idriç boundary

`idric/Pasteboard.idric` owns the first structural contract: 3 boards, 16 slots per board, 2 columns, 8 rows, middle board first. `GenerateConfig.idric` emits the C header consumed by the Android runtime, and CI rejects a checked-in generated header that no longer matches the Idriç source.

This is intentionally not a claim that the Android app is already direct Idriç → ARM/DEX. The existing buildable route is NativeActivity + NDK + JNI; using it gets an installable APK now while keeping a real Idriç-owned boundary that can move downward as the backend grows.

## Recovery and acceptance status (2026-11-01)

The source is restored unchanged in purpose from the historical `pasteboard`
branch to current utility-main integration. The JNI clipboard read/write path
now calls the merged `../android-clipboard/` bridge with explicit standard
UTF-8 lengths. The model refuses an input that exceeds its 8191-byte text
capacity instead of storing a silently truncated string; **this is not full-size
clipboard archival**, and its user-facing error handling remains to be built.
State writes use a synced private temporary sibling and atomic rename rather
than opening the committed state for truncation. APK builds require the stable,
verified shared test signer; a prior APK signed with an ad-hoc key cannot
silently upgrade under that new identity.

**This does not finish fixed Pasteboard.** Automatic capture of the *intermediate*
values copied while another app is focused remains unimplemented. Android 10+
only allows ordinary clipboard reads to the focused app or selected default IME.
The checked direct-DEX/JNI backend has advanced but has not qualified an
InputMethodService lifecycle or background listener. A separately authorized
Shizuku shell-UID listener is a candidate that could preserve an existing
keyboard, but it must pass device-level Binder, identity, event-delivery,
restart and revocation tests before being relied upon.

Deletion, pinning, deterministic pinned eviction, durable overflow/archival,
long-text storage, multi-process writer synchronization and the user-visible
capture-status indicator are still outstanding. The source requirements and
non-negotiable background-capture acceptance are recorded in
[issue #89](https://github.com/Ashtray-Archer/utilities-android-phone-user/issues/89).

For the recovery branch, `run-host-tests.sh` exercises the bounded C model.
The source workflow `.github/workflows/pasteboard-apk.yml` builds the shared
JNI bridge and APK against Android API 36, verifies stable signing, and invokes
API 29/34 emulator launch checks. Those checks cannot substitute for testing
three distinct clipboard copies made while Pasteboard lacks focus.
