# Hardware sources

This utility makes phone hardware available through a small file-like semantic namespace while keeping Android-specific mechanisms underneath it.

The namespace is synthetic for now. It does not require a Linux mount, FUSE, root, or a Plan 9 port.

## First object: accelerometer

```text
/hardware/
  sensors/
    accelerometer/
      info
      sample
      events
      control
```

The paths mean:

- `info` — report the selected accelerometer and its stable metadata;
- `sample` — read one measurement, write `x y z`, then exit;
- `events` — remain open and write measurements until stopped;
- `control` — explicit settings such as sampling period; reserved until write semantics are settled.

Accelerometer values are in metres per second squared. The native `events` stream prefixes each measurement with the Android sensor event timestamp in nanoseconds. `sample` deliberately stays the smallest useful program and writes only the three axes.

The public model is the operation on a named source. `ASensorManager`, Binder, HAL transactions, `/dev` nodes, Termux sockets, JNI, and libc details are implementation choices beneath that model.

## Native Android backend

`android-native/hardware_sensor.c` uses the Android NDK sensor API in `libandroid`.

After building, the interface is:

```text
hardware-android-native-armv7 read /hardware/sensors/accelerometer/info
hardware-android-native-armv7 read /hardware/sensors/accelerometer/sample
hardware-android-native-armv7 read /hardware/sensors/accelerometer/events 10
hardware-android-native-armv7 read /hardware/sensors/accelerometer/events
```

The last form streams until interrupted.

Build with a pinned Android NDK by setting `ANDROID_NDK_HOME` and running:

```text
sh hardware/build-android-native.sh
```

The build produces ARMv7, AArch64, and x86_64 Android executables. Build success is not a physical-phone receipt.

## Termux:API backend

`termux-api/sample.sh` implements the same one-shot `sample` meaning through the existing Termux:API bridge:

```text
sh hardware/termux-api/sample.sh
```

This is intentionally a separate backend, not a fallback that can be silently substituted for a failed native result.

The inspected references are pinned to:

- `isomorphisms/termux-api` / upstream Termux:API app: `44dff8932c23c37531bcc930d8a916f81bf8d43a`;
- `termux/termux-api-package`: `0e3f9222eea7760c76ea6368dadbdf884ab85fbf`.

At the app side, `SensorAPI` uses Android `SensorManager` and streams JSON through a local socket. The command-side `termux-sensor` script invokes the `termux-api` helper with the `Sensor` method. The current app output retains axis values but does not retain the Android event timestamp, so the Termux backend does **not** yet satisfy the timestamped `events` contract. Do not hide that difference.

## Lower-layer probes

Keep the same tiny accelerometer action available as separate investigations where feasible:

```text
accelerometer/android-api
accelerometer/binder
accelerometer/linux-device
accelerometer/hal
```

The sequence is deliberate:

1. NDK/libandroid sensor API;
2. direct Binder to `sensorservice`;
3. direct sensors HAL access;
4. kernel-facing `/dev`, `/sys`, IIO/input/vendor interfaces;
5. bus/register-level access only if the phone and process privilege actually permit it.

A permission or SELinux denial is evidence about the boundary, not a successful measurement.

`probe-phone.sh` performs a non-root, read-only reconnaissance pass over Binder nodes, service visibility, sensor diagnostics, IIO/input candidates, and obvious raw bus/register nodes. Run it from the same Termux process context that will execute the native sensor program:

```text
sh hardware/probe-phone.sh > hardware-probe.txt 2>&1
```

It does not open arbitrary binary device nodes or mutate hardware state.

## Acceptance boundary

Keep these facts separate:

1. source/build success;
2. Android executable identity and ABI;
3. emulator execution with a declared virtual/injected sensor;
4. physical-phone execution;
5. direct Binder/HAL/kernel reachability.

A working NDK or Termux:API measurement does not prove that direct Binder, HAL, or kernel access is available. Conversely, a blocked lower layer does not invalidate the stable higher-level implementation.

Tracking: Ashtray-Archer/utilities-android-phone-user#43, isomorphisms/Idric#85, isomorphisms/grease#23.
