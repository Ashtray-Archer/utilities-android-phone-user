# Sensor instruments using the continuous font

Gyroscope and Magnetometer previously contained only `.gitkeep` files. This
small shared native adapter establishes the actual acquisition-to-text path:
Android's gyroscope (rad/s), magnetic field (microtesla), or light (lux)
events → finite raw readings → unchanged units → canonical Subpixel glyphs →
the native window. It does not claim a pre-existing application was upgraded.
No calibration, smoothing, fusion or synthetic readings are introduced.

The shared source selects its Android boundary at compilation:

| Kind | Application | Reported quantity |
| --- | --- | --- |
| 1 | Gyroscope | Android x/y/z angular velocity, rad/s |
| 2 | Magnetometer | Android x/y/z magnetic field, microtesla |
| 3 | standalone Ambient Light | Android light level, lux |

All font geometry and sampling stay in `rough.framebuffer` at the pinned font
revision. The adapter only caches glyph coverage and blends text onto a window.
No bitmap glyph table or alternate font rasterizer exists in these consumers.
Missing sensors and failed activation are displayed explicitly. Sources are
disabled on loss of focus; activation drains old queued readings and waits for
new finite samples. The instrumentation logs posted-frame duration and cached
glyph bytes; the workflow separately records APK sizes and emulator memory.

The existing host packaging interface accepts `gyroscope` or `magnetometer` as
its application selector. Kind 3 uses an explicitly supplied standalone
`AMBIENT_LIGHT_ROOT`, preserving separate repository ownership and package ID.
Maintained C is produced by ICK; NDK owns upstream glue, assembly and linking.
This adapter does not establish Idriç compiler generation or execution.

The workflow builds ARMv7, ARM64 and x86-64 APKs, inspects identity and ELF
contents, then checks same-candidate replacement, process launch and a posted
frame on an emulator. Those are configured checks until their actual run passes.
No physical A1/C67 result or sensor accuracy follows from emulator acceptance.
