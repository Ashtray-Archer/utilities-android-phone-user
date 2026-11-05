# Charcoal sensor instruments

Accelerometer, Spirit Level, Gyroscope and Magnetometer adopt the shared
`isomorphisms/android-NDK@3e682fb1175beef5625dbd2418730220e49fc9bd`
charcoal contract. The common instrument adapter is also used by standalone
Ambient Light; that consumer must adopt this source and theme resources too.

Build hosts materialize that exact checkout at `.charcoal`. The native code
includes its narrow public color header. CI copies its existing Android
resources into each app's resource input before using the existing direct
packager. Generated copies are ignored, rather than maintaining another
independent palette. Standalone build hosts need the same pinned dependency
and resource materialization; phones are consumers of finished APKs.

The native canvas uses #212121, primary text #F4F4F4 and secondary text #B4B4B4.
Accelerometer's decorative track uses the border role. Spirit Level retains
its mathematical geometry and a distinguishable success-colored marker; its
reference guide uses the secondary role for legibility. Sensor acquisition,
numeric storage/formatting, glyph geometry, package/version/signing identity,
fullscreen policy and interaction behavior are preserved.

This branch is stacked on the active shared-instrument font implementation.
It does not overwrite the Pasteboard theme branch or merge the font work.
Existing ICK source production, native build, package, host geometry and
same-candidate replacement checks remain in force. Signed builds, API29/API34
visual/state/rotation checks and physical MIRO C67 acceptance are pending.

The instrument workflow's expired Grease artifact is replaced with a retained
artifact containing the identical verified runtime binary, SHA256
7e31cd05b7a9d8fb2a4a9e003a7f3fcb0159138506d17f0fb28da8cbe22aa85c.
The public launcher and its digest are unchanged. No alternate shell or
compiler is substituted; no release or installation is performed.
