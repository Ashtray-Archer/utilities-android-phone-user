# Compact-state follow-up allocation

## Decision and evidence

- Sun implementation commit: `80bcbead99e9727f00bbecffbe3f223feebeb105` on `accelerometer/compact-geometric-state`.
- The three-byte Q0.7 octahedral design was rejected by a deterministic 8,660,069-sample study: maximum component error `0.538368 m/s²` exceeds the screen quantum `1/7 m/s²`.
- The implemented four-byte Q0.11 octahedral direction plus exact magnitude byte has measured maximum component error `0.077181 m/s²`; its retained record is mechanically asserted to occupy four bytes.
- The host codec, invariants, retained-state-only UI model, seventh formatter, and error study pass at the implementation commit.
- Android SDK/NDK packaging, CI, installation, launch, visual layout, and raw-versus-decoded MIRO A1 behavior are not established by those host results.
- [Ashtray-Archer/utilities-android-phone-user PR #58, “Supersample whole accelerometer glyphs”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/58) merged into `main` at `bf44102c1cec8b27498fc98410a418be8e2a66c8` while Sun was running. The compact-state branch was rebased onto that exact main head, so the whole-glyph supersampling is preserved.

## Recommended allocation

- Sun: done unless new evidence contradicts the four-byte error envelope.
- Earth: one bounded Android build and physical-oracle-preparation pass after the Sun commit is remotely accessible.
- Moon: mechanical glyph fixtures, documentation reconciliation, and evidence recording after Earth supplies one exact green head and the human supplies any required physical evidence.

## Earth follow-up job

Integrate and build the settled compact accelerometer state in `Ashtray-Archer/utilities-android-phone-user`.

This is an Earth job. The representation decision belongs to Sun and is fixed by commit `80bcbead99e9727f00bbecffbe3f223feebeb105` on branch `accelerometer/compact-geometric-state`, rebased onto `main` at `da6bea23fc5740e696181181685e68082aaf1934`. Before changing anything, fetch the repository, verify that exact commit exists, and inspect its full diff and host-test output. If the Sun commit is not remotely accessible, stop at that blocker rather than recreating it from this prose.

Outcome: produce one exact branch head and APK build that preserve the settled four-byte Q0.11 octahedral codec, magnitude meaning `m/(4√3)`, decoded-only screen path, nearest-seventh presentation, merged whole-glyph supersampling, current large layout, stable signer, and native DEX-free architecture. Do not redesign the representation, return to Q0.7, reintroduce `_Float16` shadow axes, change the magnitude quantum, or use raw Android values for presentation.

Run the committed host tests and deterministic error study. Build the ARMv7, AArch64, and x86_64 APK through the repository's existing Android SDK/NDK path; inspect the finished APK, absence of `classes.dex`, ARMv7 Thumb attributes, signer, and replacement-install CI stage. Keep build, emulator, and physical-phone evidence separate. Ensure transient raw and decoded values remain available in the Android log for later MIRO A1 comparison, but do not claim a physical result.

Stop and return to Sun if current renderer work changes the codec boundary, if the measured maximum component error reaches `1/7 m/s²`, if retained state exceeds four bytes, or if integration would require a new geometric decision. Do not merge, close, publish a release, or mark physical acceptance complete.

Finish with the exact branch head, full test/build results, APK artifact identity if built, and one bounded physical MIRO A1 acceptance procedure or a concrete blocker.

## Moon follow-up job

Propagate the verified compact accelerometer state mechanically after Earth provides one exact green source head descended from `80bcbead99e9727f00bbecffbe3f223feebeb105`.

This is a Moon job. Before acting, verify the supplied exact head exists, contains the four-byte Q0.11 codec, passes `accelerometer/tests/compact_acceleration_test.c` and `accelerometer/tests/error_study.c`, and has a successful APK build. Reject a stale head, a three-byte/Q0.7 reversion, a raw-value renderer, or an unresolved semantic conflict.

At that exact prerequisite only:

1. Add fixed framebuffer/glyph fixtures for integral values and numerators `1` through `6`, including positive and negative proper fractions, mixed fractions, and the integral simplification that omits `0/7`.
2. Propagate the settled four-byte/Q0.11 description through remaining accelerometer fixtures and documentation; remove obsolete claims that production retains `_Float16 x/y/z`, that `40/7` defines the physical reference, or that `/256` defines residual scale. Preserve historical material when deletion would erase why it was rejected.
3. Record exact-head host, build, artifact, signer, and emulator results in plain receipts without promoting them to physical-device evidence.
4. If the human supplies a MIRO A1 raw-versus-decoded log and visual result for that exact APK, record it as physical evidence with artifact digest and exact source head. Otherwise leave physical acceptance explicitly unresolved.

Do not alter the representation, choose new glyph geometry, change layout proportions, merge, release, or infer a physical pass. Stop for Sun/Earth if fixture propagation reveals semantic divergence or if the settled renderer cannot express a required seventh unambiguously.
