# Accelerometer

Show the phone's live accelerometer reading as an ordinary Android application.

The first screen continuously displays reconstructed x, y, and z acceleration components in m/s². Android's binary32 sensor sample is encoded into the compact geometric state, decoded, and only then rounded to the nearest seventh for the screen. Android's accelerometer includes gravity, so a stationary phone should normally show a vector whose magnitude is close to 9.8 m/s² rather than three zeros.

## First Android slice

This first implementation is deliberately small and native:

- ordinary launcher APK;
- `android.app.NativeActivity`;
- no `classes.dex`, Java, Kotlin, Gradle, or Compose;
- Android NDK `ASENSOR_TYPE_ACCELEROMETER` input;
- direct native-window rendering of the changing numeric values as mixed/vulgar-style sevenths;
- a four-byte retained geometric state after Android's float-valued sensor event crosses the platform boundary;
- no retained binary32 or `_Float16` x/y/z shadow state: the screen reconstructs from the compact bytes;
- ARMv7 (`armeabi-v7a`), AArch64, and x86_64 builds from the same source;
- the ARMv7 library is compiled explicitly as Thumb code.

The C implementation is a platform oracle, not the intended permanent owner of the application semantics. The Android boundary now lives in `android/android_accelerometer.[ch]` and names its output an **Android accelerometer reading**: the timestamp plus three binary32 acceleration components reported through Android's sensor stack in m/s². Those values are not ADC counts or sensor-chip register contents.

The native screen and the command-line `hardware-android-native` utility compile that same acquisition source. The latter exposes one-shot `sample` and streaming `events` output on stdout, so Grease and other programs can consume the Android accelerometer reading without launching the graphical application. It also exposes `inspect` and `inspect-events`, which run the reading through the same shared `model/` code used by the instrument and report the balanced-gravity difference, compact residual direction as a unit pure quaternion, compact bytes, reconstruction, and seventh-based screen values. Above the Android boundary, the screen and command line therefore share the mathematics rather than reimplementing it.

[`COMPACT-STATE.md`](COMPACT-STATE.md) defines the balanced reference, residual codec, explicit overflow and malformed-state behavior, rejected three-byte design, and deterministic reconstruction-error study. The Android accelerometer reading remains available transiently for comparison logging; it is not retained application state and does not feed the renderer.

## ARM/Thumb handoff

The matching ARM/Thumb compiler target is: generate the sample-producing native core directly from Idriç and link it behind the same Android sensor/platform boundary. The Android application must not be counted as ARM/Thumb compiler evidence merely because its current C oracle is compiled to Thumb-2.

The replacement gate should separately establish:

1. an Idriç source program owns the sample semantics;
2. the ARM/Thumb backend emits the claimed ARMv7 Thumb-2 object for that source;
3. the Android adapter supplies sensor events without duplicating the Idriç-owned logic;
4. the APK links the generated object;
5. emulator/build evidence and physical-phone sensor evidence remain distinct.

## Build

With Android SDK platform 36, build-tools 36.0.0, and NDK 27.2.12479018 installed:

```sh
./accelerometer/build-apk.sh
```

The output is `accelerometer/app/build/outputs/apk/debug/app-debug.apk`.

No special Android permission is required for the ordinary accelerometer.
