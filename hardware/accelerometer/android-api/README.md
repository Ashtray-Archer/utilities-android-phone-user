# Accelerometer through the Android NDK API

Status: **IMPLEMENTED, NOT YET RUN ON THE PHYSICAL PHONE**.

The implementation is [`../../android-native/hardware_sensor.c`](../../android-native/hardware_sensor.c). It uses `libandroid` through `ASensorManager`, `ASensorEventQueue`, and `ALooper`.

Canonical one-shot behavior:

```text
read one accelerometer measurement
write x y z in m/s^2
exit
```

This layer is the stable native Android boundary. It is distinct from a direct Binder, HAL, or kernel-device implementation even though those lower mechanisms may ultimately carry the same measurement.
