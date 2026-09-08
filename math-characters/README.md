# Programmer's Unicode Picker

A small standalone Android picker for entering symbols and text macros from the Programmer's Keyboard layouts. Tap keys into the buffer, tap **COPY**, switch to the target app, and paste. It is an ordinary app, not an Android input method or system keyboard.

## Idriç refactor boundary

The first refactor slice moves all eight page definitions and their allowed key actions into `idric/UnicodePicker.idric`. The Idriç type only permits text insertion, paired insertion, and the three pastebin operations; movement keys, signals, and IME actions cannot be placed on a picker page. Each pastebin action carries one of the closed `paste_slot` choices rather than an arbitrary natural number. The C snapshot still receives a zero-based unsigned index and defensively checks it before array access.

`idric/GenerateLayouts.idric` emits the deterministic C layout snapshot compiled into the APK. `check-idric-layouts.sh` compiles the Idriç model, runs its picker-specific contract, rejects a stale generated snapshot, and proves that a raw natural number cannot be supplied as a paste slot. The mutable editing state and Android boundary remain C for now; this slice does not disguise handwritten C as Idriç or ship a Scheme runtime on the phone.

The APK contains native C machine code only:

- no Java or Kotlin source
- no generated `classes.dex`
- no app classes or inheritance
- no network or Android permissions
- `arm64-v8a`, `armeabi-v7a`, and `x86_64` native libraries

Android's platform-provided `NativeActivity`, `Canvas`, and clipboard service supply the OS boundary. The checked-in generated layouts, mutable editing state, touch handling, rendering orchestration, and clipboard call site compile as C.

## Pages

The first page keeps the earlier Math Characters set: arrows, number sets, ordinary digits, superscripts, and subscripts. It now also carries the confirmed long, mapsto/from-bar, hook, two-headed, and repeat arrows.

The other seven phone pages are:

1. Math
2. Punctuation
3. Programming
4. Regular Expressions
5. Concept Separation
6. Incantation Assistance
7. Several Pastebins

The layouts were transcribed from `isomorphisms/programmers-keyboard` at commit `43f900c61a3f03d612e19d5cbacbab820b5c0dc3`, plus the punctuation note at `410d2a2`. Punctuation keeps mathematical minus `−`, en dash `–`, and em dash `—` distinct; ASCII hyphen-minus is deliberately not given a key. Quad and thin spaces remain separate. Lambda and useful arrows intentionally appear on more than one page.

Movement and Signals remain hardware-keyboard concerns and are omitted from the phone picker. Incantation Assistance keeps Tab/Complete and Finish Incantation, but not chant history.

Paired delimiters and quote marks insert both characters and leave the cursor between them. Pastebin slots last for the current app process.

## Why this is a picker, not an IME

Android requires a software keyboard service to inherit from `InputMethodService`, which is a Java framework class loaded from DEX bytecode. A true IME therefore cannot also satisfy “no Java/Kotlin, no classes, no virtual-machine bytecode.” This build is deliberately a standalone Unicode picker with a buffer and clipboard operation.

## Tests

Run the platform-independent behavior suite on Linux:

```sh
./run-host-tests.sh
```

With the pinned Idriç compiler available, verify the typed source and generated boundary:

```sh
PATH=/path/to/Idric/.tools/bin:$PATH \
IDRIC_COMPILER=/path/to/Idric/build/exec/idris2 \
./check-idric-layouts.sh
```

It compiles with strict warnings plus address and undefined-behavior sanitizers. The tests cover:

- every page, row, label, output, and UTF-8 string
- Unicode-aware insertion, cursor movement, and backspace
- paired insertion and exact quad/thin/ordinary spaces
- punctuation distinctions, duplicated λ/arrows, undo, completion keys, and pastebin slots
- buffer-capacity failure without partial writes
- touch hit-testing for every key in portrait and landscape

The Android workflow then builds the APK, verifies that it is signed, aligned, permission-free, DEX-free, and contains every target ABI, and launches it in an API 29 emulator. The emulator taps a symbol, copies it, changes pages, taps a second key, captures the screen, and checks that the native process remains alive without a fatal log entry.

## Build

The build directly composes NDK Clang, `aapt2`, `zip`, `zipalign`, and `apksigner`. It needs JDK 17 for the Android signing program, Android SDK/build-tools 36, and NDK 27.2.12479018:

```sh
./build-apk.sh
```

The debug APK is `math-characters/app/build/outputs/apk/debug/app-debug.apk`. Pull-request runs upload it as `programmers-unicode-pad-debug-apk`. Set `ANDROID_KEYSTORE` when rebuilding an installed copy so Android recognizes the new APK as an update.
