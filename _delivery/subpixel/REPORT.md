# Sensor font acceptance — SUN SUBPIXEL-FONT-ANDROID-01

Status: **INCOMPLETE — remaining application CI and physical acceptance pending.**
No implementation PR has been merged.

All six named applications now have source paths using the same continuous
5×7 font at `83ea1bf4989a8857b9d70c9ae238aa9e0932bf0e`. Existing bitmap reconstruction has been
removed from Accelerometer and Proximity. Spirit Level keeps its existing
geometry and seven-pixel footer. Gyroscope, Magnetometer and Ambient Light
previously lacked implementations; their narrow Android sensor-to-text adapter
uses actual platform readings and explicit unavailable/error states. No copied
font geometry or alternate Subpixel renderer was added.

## Results by application

| Application | Build and package | Emulator | Physical A1/C67 | Source |
| --- | --- | --- | --- | --- |
| Accelerometer | PASS ARMv7, ARM64, x86-64; expected signer | PASS same-candidate replacement and launch | NOT_RUN | `b4587cf189587456b1026226be3195429c905735` |
| Proximity | PASS ARMv7, ARM64, x86-64; expected signer | NOT_RUN | NOT_RUN | `50832639a748f1c2b4f1000cf44a31f4977d0320` |
| Spirit Level | PASS ARMv7, ARM64, x86-64; expected signer | PASS same-candidate replacement, launch and posted frame | NOT_RUN | `4ff24e66ee784ab734f66d1ffb28e1420153e640` |
| Gyroscope | PENDING current-head CI | PENDING | NOT_RUN | `e892720ee0d9814b99d4b1c4cdd650c43de71a0a` |
| Magnetometer | PENDING current-head CI | PENDING | NOT_RUN | `e892720ee0d9814b99d4b1c4cdd650c43de71a0a` |
| Ambient Light | PENDING current-head CI | PENDING | NOT_RUN | `26319fbb8e0e9120bf96655e9d1fae6d7a337b82` |

Same-candidate replacement is not an observed upgrade from an arbitrary older
APK. Package IDs and persistent signer are retained for existing applications.
The new applications establish initial package identities and versionCode 1.
All maintained application/font C is ICK-produced; NDK owns upstream glue,
assembly and linking. These results do not establish Idriç generation/execution.

## Exact downloadable candidates

| Application | MIRO A1 — ARMv7 | MIRO C67 — ARM64 |
| --- | --- | --- |
| Accelerometer | [APK](https://github.com/Ashtray-Archer/utilities-android-phone-user/raw/299986cb3fc369f9231fe8773a463ca6f7d40b7d/_delivery/subpixel/accelerometer-A1-armeabi-v7a.apk) | [APK](https://github.com/Ashtray-Archer/utilities-android-phone-user/raw/299986cb3fc369f9231fe8773a463ca6f7d40b7d/_delivery/subpixel/accelerometer-C67-arm64-v8a.apk) |
| Proximity | [APK](https://github.com/isomorphisms/Android-proximity-sensor/raw/59f9d093a3557c943eab3f44887743fcd8383857/_delivery/subpixel/proximity-A1-armeabi-v7a.apk) | [APK](https://github.com/isomorphisms/Android-proximity-sensor/raw/59f9d093a3557c943eab3f44887743fcd8383857/_delivery/subpixel/proximity-C67-arm64-v8a.apk) |
| Spirit Level | [APK](https://github.com/Ashtray-Archer/utilities-android-phone-user/raw/299986cb3fc369f9231fe8773a463ca6f7d40b7d/_delivery/subpixel/spirit-level-A1-armeabi-v7a.apk) | [APK](https://github.com/Ashtray-Archer/utilities-android-phone-user/raw/299986cb3fc369f9231fe8773a463ca6f7d40b7d/_delivery/subpixel/spirit-level-C67-arm64-v8a.apk) |

`artifacts.tsv` binds bytes, SHA-256, source, ABI and build run for these files.
CI verified the expected certificate:
`de9b1d47c5a65e6d46a204b79dd9ee566b9d3c9832ba81ebc4213d3392e92ff9`.

## Numerical and build evidence

- [Exact-head pinned ICK/Ike font/core qualification — PASS](https://github.com/isomorphisms/rough.framebuffer/actions/runs/40026131502).
- [Independent current-head numerical acceptance — PASS](https://github.com/isomorphisms/ai-ci/actions/runs/40030032035): 100 exact finite-segment/Boolean-union cases, 32 sampled contour cases, and 710 visible glyph/scale calls. Historical bumpy slash control rejected with the intended diagnostic and exit 1.
- Existing independent font verifier: 76 cases, 463 assertions, 0 failures on host.
- Spirit Level host acceptance: 10,656 retained-state geometry/rotation cases plus pixel, clipping and buffer checks PASS.
- [Accelerometer package/replacement run — PASS](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/40026397324).
- [Proximity package run — PASS](https://github.com/isomorphisms/Android-proximity-sensor/actions/runs/40026676822).
- [Spirit Level package/emulator run — PASS](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/40027149000).
- [Gyroscope/Magnetometer current-head run — PENDING](https://github.com/Ashtray-Archer/utilities-android-phone-user/actions/runs/40028889807).
- [Ambient Light current-head run — PENDING](https://github.com/isomorphisms/ambient-light/actions/runs/40028976065).

Before/after slash specimen: [literal bitmap, old reconstruction, continuous stroke](https://github.com/isomorphisms/ai-ci/blob/90d11ce51ec20d1db651aa7f1c67fb4d6dc703f4/subpixel/sensor-font/slash-before-after.png).
[Complete alphabet specimen](https://github.com/isomorphisms/rough.framebuffer/blob/83ea1bf4989a8857b9d70c9ae238aa9e0932bf0e/subpixel/examples/previews/sensors/alphabet.png).

Host tests establish bounded mathematical geometry/coverage and sampled
topology. They do not prove physical screen quality, calibration, sensor
availability, cross-ABI mask identity or device performance.

## Reviewable changes

- [isomorphisms/rough.framebuffer PR #24, “Share continuous 5×7 instrument glyphs through the whole-path font API”](https://github.com/isomorphisms/rough.framebuffer/pull/24).
- [Ashtray-Archer/utilities-android-phone-user PR #83, “Use the shared continuous Subpixel font for Accelerometer text”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/83).
- [isomorphisms/Android-proximity-sensor PR #3, “Render Proximity text with the canonical continuous Subpixel font”](https://github.com/isomorphisms/Android-proximity-sensor/pull/3).
- [Ashtray-Archer/utilities-android-phone-user PR #84, “Use shared continuous Subpixel typography in Spirit Level”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/84).
- [Ashtray-Archer/utilities-android-phone-user PR #85, “Connect Gyroscope and Magnetometer to the shared continuous font”](https://github.com/Ashtray-Archer/utilities-android-phone-user/pull/85).
- [isomorphisms/ambient-light PR #1, “Build Ambient Light with the shared continuous Subpixel instrument”](https://github.com/isomorphisms/ambient-light/pull/1).
- [isomorphisms/ai-ci PR #325, “Independently check continuous sensor-font strokes and contours”](https://github.com/isomorphisms/ai-ci/pull/325).

## Remaining acceptance

1. Complete exact-head Gyroscope/Magnetometer/Ambient Light CI, inspect their finished APKs and publish the A1/C67 candidates.
2. Exercise Proximity replacement/launch; do not infer it from package construction.
3. Complete per-application rendering-time, memory and responsive-update measurements. APK sizes are measured; the generic adapter logs frame time/cache bytes and its configured emulator job records memory, but those results are pending. No device-performance PASS is claimed for the other apps.
4. Observe exact APKs on physical A1/C67: replacement without uninstall, glyph appearance (especially /), numbers/units, readings, orientation and pause/resume.
5. Reconcile outstanding receipts and merge only after the applicable gates pass.

Other named native utilities checked: isomorphisms/beep and isomorphisms/shine
are empty repositories, so no existing renderer can be upgraded there.
GitHub indexed search did not identify another working copy of the legacy
glyph reconstruction; indexing is not a complete cross-repository audit.
