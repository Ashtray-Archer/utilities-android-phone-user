# Spirit level

A minimal level line and a small tilt marker. Raw Android `x`, `y`, and `z`
appear only in seven-pixel-high diagnostic text at the bottom. A second tiny
line states whether the screen faces up/down or a reading is unavailable.
The footer stays inside Android's content rectangle, above system bars.

This is the first implementation slice for issue #64, not a calibrated level
or a full attitude instrument. It uses the **decoded acceleration directly**.
There is no added quaternion, gyroscope, magnetometer, gravity sensor, or fused
orientation sensor. A quaternion derived from this one direction would not
supply an extra measurement or recover yaw.

## One acquisition implementation, two graphical consumers

```text
physical accelerometer -> Android sensor stack
    -> accelerometer/android/android_accelerometer.c (existing shared adapter)
    -> accelerometer_model_accept (existing shared four-byte retained state)
    -> accelerometer_model_reconstruct (existing shared decoder)
    -> spirit_level_geometry_make -> spirit_level_draw -> native window
```

The Android callback also formats its transient raw binary32 values into a
64-byte text cache for diagnostics. Those strings are never parsed back into
numbers or used by the geometry. The level retains neither a legacy Float16
x/y/z triple nor a second numerical copy of the Android reading. The producer,
codec, model, and accelerometer application's display logic remain unchanged.

`native_main.c` owns lifecycle and window handling, and calls only the shared
adapter for acquisition. It disables the source on pause/focus loss, drains
queued wakeups without displaying them while paused, rejects events timestamped
before reactivation, waits for a new accepted reading on resume, and closes the
source at shutdown. JNI reads **display rotation metadata**, not a sensor.
Unknown display rotation is an unavailable state, never a guessed portrait.

## Retained state to screen geometry

Let `a` be the existing decoder's reconstructed acceleration, **after adding
back the balanced reference**. The compact residual direction by itself is not
gravity and is not the level's input. In particular, the canonical zero residual
still decodes to balanced gravity; it is not a zero acceleration measurement.

Android keeps its sensor axes in the device's natural orientation. For display
quarter turns 0, 1, 2, 3, map the decoded x/y to screen right/up as:

```text
0: ( x,  y)       1: ( y, -x)
2: (-x, -y)       3: (-y,  x)
```

This handles natural-landscape devices as well as phones. The normal component
`z` does not change. Define `r = hypot(right, up)` and `m = |a|`.

The level line direction in right/down pixel coordinates is `(up/r, right/r)`.
Its dot product with projected support acceleration `(right, -up)` is zero.
Clip that line through the viewport center to a rectangle above the footer, in
**pixel coordinates**, so portrait/landscape aspect ratios cannot stretch the
angle. No slope or division by one component is needed for a vertical line.

The marker position relative to center is a fixed pixel radius times
`(right/m, -up/m)`. It moves toward the high side at rest, like a bubble. This
also makes out-of-screen tilt visible rather than discarding z after calculating
an in-plane angle. Inclination `atan2(r, z)` spans face up through edge-on to face
down (0 through pi), without a hemisphere cut. The marker is filled face up and
outlined face down; the tiny status text distinguishes the hemispheres too.

When `r` is no larger than one codec magnitude quantum, the line becomes a dim
horizontal reference and the status says `FLAT`. Near either pole a unique
in-screen level direction is undefined; do not amplify quantization noise into
a spinning line. The marker continues to use decoded tilt. This deadband is a
presentation convention, **not** an angular accuracy or calibration guarantee.

Missing samples, malformed state, nonfinite readings, unknown rotation, and
codec saturation show an unavailable status with only a dim reference guide.
A reconstructed magnitude at or below 0.5 m/s² also has no usable direction for
this display. This threshold is not a free-fall detector. Rejected readings do
not silently display the last valid tilt as current.

An accelerometer measures support/acceleration, not isolated gravity during
arbitrary motion. Treat this as a quasi-static level. Linear acceleration,
bias, noise, mount alignment, and codec quantization limit accuracy. Yaw about
gravity is not observable from this input. No calibration, filtering, sensor
fusion, or precision/physical-device claim is introduced in this slice.

Android coordinate reference:
https://developer.android.com/develop/sensors-and-location/sensors/sensors_overview#sensors-coords

## Synthetic acceptance

Run the host-only check from any current directory, using the actual checkout
location (the example below is the build host, not a phone installer):

```sh
sh /opt/utilities-android-phone-user/spirit-level/tests/run.sh
```

The tests compile the same shared codec/model, geometry, and pixel renderer as
the APK. They cover six cardinal poses, display-rotation signs, changing z,
10,656 full-sphere/rotation cases, portrait/landscape/tiny viewport clipping,
flat poles, zero acceleration, canonical zero residual, nonfinite input,
saturation, malformed state, missing samples and unknown display rotation.
Pixel checks require a visible line/marker, reserve the footer, protect stride
padding and allocation guards, and prove changing diagnostic text cannot change
the main display. A source check rejects a second acquisition implementation in
the spirit-level code and checks its shared-adapter calls.

The initial GCC and Clang address/undefined-behavior-sanitizer runs passed.
The 9.81 m/s² sphere grid's largest inclination error against its synthetic
input was 0.006893 radians (about 0.395 degrees). That is a **sampled codec plus
geometry result**, not a global bound and not phone accuracy. Tests constrain
this grid to less than 0.02 radians; no input readings bypass the codec.

## Native APK and delivery boundary

`build-apk.sh` delegates to the existing native builder with the `spirit-level`
consumer selected. The builder compiles the original shared Android adapter,
uses the existing persistent public test signer, and keeps version code 1 for
this first version. Package identity is `com.ashtrayarcher.spiritlevel` and the
native library is `libspirit_level.so`. No application Java/Kotlin/DEX or Gradle
is introduced. Do not substitute a new signer or uninstall to make replacement
installation appear successful.

Build inputs remain Android SDK platform/build-tools 36 and NDK 27.2.12479018.
Build on the host, not the phone. Distribution outputs under
`app/build/outputs/apk/distribution/` include stripped single-ABI APKs:
`spirit-level-armeabi-v7a.apk` for the ARMv7 phone, `spirit-level-arm64-v8a.apk`
for AArch64 devices, and `spirit-level-x86_64.apk` for the emulator. The builder
also retains universal debug/distribution variants; they are not the preferred
phone download.

The workflow `spirit-level native slice` checks out the exact PR head, runs host
and sanitizer checks, builds/checks signed native packages, and exercises
same-candidate replacement installation plus a first posted frame on an Android
emulator. Every material `accelerometer/**` dependency can trigger it. Artifacts
include the candidate revision in their names and retain separate evidence for
each stage. A workflow definition is not evidence that those stages have run.

Physical acceptance remains separate: install the exact stripped ARMv7 APK on
the phone, confirm the level line and raw diagnostic footer, exercise upright,
sideways, face-up/down and shallow tilt, background/resume, and record the exact
revision/APK hash with the observations. There is no claim here of a physical
sensor-to-screen run, calibrated accuracy, or upgrading an arbitrary older APK.
