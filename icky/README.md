# Division glyph source producer

Maintained C expressions use `÷` directly. ICK `c61e448251744a2f40ad743ebef1a027bdcd2f9d` compiles them without a source rewrite. Shared producer and checked Android header adapters are pinned at `isomorphisms/ai-ci@4ea071a96239f3a29ca6d98454feb59947d87cfe`.

`Host.mk` retains the existing compact-vector, acceleration/error-study, sanitized picker, UTF-8 conversion and JNI header contract tests. `ICK` must name the qualified native driver. Its installed runtime closure supplies the host runtime libraries; the driver's existing link contract is `-fno-link-libatomic`.

`Android.mk` compiles every maintained C translation unit to assembly with ICK. NDK r27c assembles and links those objects and compiles unchanged NativeActivity glue. The four existing launchers call these fixed producer rules. Their original API floors remain accelerometer/picker 26, hardware 21 and clipboard 24. Accelerometer retains ARM Thumb-2/NEON. Other ARM sources retain their original ARM/NEON target. ARM64 and x86-64 payloads remain present.

The accelerometer and picker preserve `_FORTIFY_SOURCE=2`, stack protection, warning errors and debug information. The shared adapter retains checked calls for supported Bionic functions and rejects unsupported fortified functions. Clipboard and hardware retain their existing unfortified API24/API21 profiles. No API is raised and no hardening flag is removed. The existing deliberate sensor-manager fallback has equivalent scoped GCC and Clang deprecation pragmas.

Reusable compiler jobs qualify three installed ICK stages and preserve their receipts. APK jobs restore the stages under `build/ick/<abi>` and check out the shared producer under `.ai-ci-ick`. For local builds, provide those exact stages or explicitly select an already qualified `ICK_COMPILER` and its support flags; the Makefile rejects a mismatched target. There is no Clang fallback for maintained C.

The original universal and per-ABI APKs, AAB, mathematical assertions, Idriç layout generator and emulator checks remain. The clipboard smoke APK now uses the repository's required persistent public Wegert test signer, verified by certificate digest, instead of generating a temporary key. This establishes a repeatable signer for that smoke package; it does not claim replacement of a previously generated disposable-key APK.

## Local evidence

All twelve profile/ABI combinations compile and link successfully against actual NDK r27c. Library/executable digests and the exact changed source digests are recorded in `local-native-sha256.txt` and `source-sha256.txt`. Hardware outputs are PIE executables, despite the local receipt filename suffix.

All five compact/model tests, both ASan/UBSan picker tests and UTF conversion pass. JNI syntax is checked against OpenJDK 17.0.20+8's actual public JNI headers. A failed compact-vector encode now still records its existing assertion failure and skips decoding unwritten output; no assertion or error threshold was removed.

These local proofs cover source compilation, platform linkage and host execution. Hosted exact-head package/emulator results and physical-device replacement acceptance are separate checks.
