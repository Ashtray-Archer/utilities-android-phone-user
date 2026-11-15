# Remaining recipe application acceptance

Follow source commit `7a37ff999080152aa0b6da0f87538f32f3c8e948` in
`Ashtray-Archer/utilities-android-phone-user`, branch `recipe/idric-shopping`.
The implementation hashes, 51 host checks, six mutation controls and DEX probe
receipt in this directory belong to that source. No earlier APK qualifies it.

| Identified target / boundary | State | Required evidence |
| --- | --- | --- |
| Actual DEX/ART recipe runtime | NOT_RUN | Independently call all four exported probes with positive, empty, boundary and changed inputs; compare host expectations and reject compiled semantic mutants. Emission/disassembly is insufficient. |
| Android recipe application | NOT_IMPLEMENTED | Integrate checked recipe editing, source links, durable saved recipes, pantry, store overrides, matches and cart review into the native app. Keep C/JNI to platform adapters; complete a real recipe-to-cart interaction. |
| MIRO A1, armeabi-v7a | NOT_RUN | Build and qualify exact-source recipe APK, then observe install/replacement, launch, recipe/source editing, separate-store carts and lifecycle behavior on the physical A1. |
| MIRO C67, arm64-v8a | NOT_RUN | Separately build and qualify exact-source recipe APK, then observe those same behaviors and replacement on the physical C67. |

Retain package `org.isomorphisms.greengrocer`, the registered persistent test
certificate, and a nondecreasing version code. Device checks consume prebuilt
APKs through the established delivery path. The current runtime probe has no
installable APK and does not authorize a physical PASS.

Live video ingestion and retailer authentication/catalog/cart remain separate
implementation and authorized-account acceptance work. Stored video links do
not imply access or extraction. Handoff proposals are not retailer writes or
orders. The whole reviewed plan must remain blocked if any store is unresolved.
