# Direct Android recipe execution

This is execution evidence for the checked recipe code, separate from an
installable recipe application, Android file I/O and physical-device behavior.

## November 18 runner repair

Application source: `e54a21314e7719b7255c813a03a64833280f9833`.
Frontend: `2bce7677f513d711875e978bd6f5ad28b9e54d58`.
Canonical DEX backend: `6ab4818219efa7ea5eda137ca6b76976eb2a4d42`.

The preceding Android jobs failed before loading recipe code because their
fresh runners lacked `rg`. The repaired workflow provisions and verifies that
dependency separately in each Android job.

[Completed run 44405484776](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44405484776)
establishes these boundaries:

| Target / boundary | Observed result |
| --- | --- |
| Host preparation, job 135159454362 | PASS: unchanged C cart through qualified ICK, 51 recipe cases, 24 storage cases, nine executed semantic-mutant rejections and direct DEX emission |
| API29 x86_64 ART, job 135162815074 | All 51 recipe cases PASS with output matching the host; separate changing-input probe exits 137 |
| API34 x86_64 ART, job 135162815153 | All 51 recipe cases PASS with output matching the host; separate changing-input probe exits 137 |
| Storage checks and semantic mutants on ART | NOT_RUN: each observer stopped at the failed changing-input probe |
| Full recipe/C differential | NOT_RUN: executing the unchanged C specimen is an independent control |
| Recipe APK, Android file I/O, physical A1/C67 | NOT_RUN |

The probe emitted no stdout and only `Killed` on stderr. Exit 137 alone does
not identify the responsible operation or establish a compiler defect. The
observer now records call markers, its Android PID, stage-specific logcat,
stdout, stderr and exact process exits before checking assertions.

Raw prepared sources, compiler, DEX, source/output digests and host results are
in [artifact 14475600288](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44405484776/artifacts/14475600288).
Device identities, byte-checked transferred DEX and process outputs are in
[API29 artifact 14476875025](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44405484776/artifacts/14476875025)
and [API34 artifact 14474419811](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44405484776/artifacts/14474419811).
These hosted artifacts may expire; their presence does not upgrade any
unexecuted boundary to PASS.

## Diagnosed large-count failure

Diagnostic source: `3aab5c619b8491d21153b4de2f3c030d3a80c72b`.
[Run 44407637442](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44407637442)
again passed host preparation. Both Android callers reached
`CALL packages_for_grams 2147483647` after the five smaller cases passed, then
exited 137.

The API34 observer printed PID 2208. Its logcat identifies that exact PID's
`java.lang.StackOverflowError: stack size 8188KB`, followed by repeated
`Idric.Generated.__idricManaged33(Unknown Source:193)` frames. The generated
plan binds `__idricManaged33` to `Data.Nat.div'`. This identifies recursive
division rather than an inferred loading/verifier failure. The raw diagnostic
is in [API34 artifact 14476453909](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44407637442/artifacts/14476453909);
the other caller's evidence is in [API29 artifact 14477825662](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/44407637442/artifacts/14477825662).

The repair uses arbitrary-precision Integer division with the positive divisor
already constructed by `ceiling_ratio`, then returns the nonnegative exact
Number quotient. The existing maximum-Int32 Android expectation remains
7158279. A new independent domain case expects 61489146912365172057 packages
for 18446744073709551617000 units in 300-unit packages, exercising a result
above uint64. The rounding-down mutant must still fail its original 500 g
assertion. The host scalar control now also includes negative and maximum-Int32
demand.

Current-head repair qualification is recorded in
[Ashtray-Archer/utilities-android-phone-user PR #88, “Implement Idriç recipe planning and exact saved recipe files”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/88).
The historical checkpoint above is not promoted to a PASS for the repair.
