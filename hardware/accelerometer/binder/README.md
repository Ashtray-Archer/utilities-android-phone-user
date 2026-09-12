# Accelerometer through Binder

Status: **NOT IMPLEMENTED / NOT CLAIMED REACHABLE**.

Target experiment: obtain one accelerometer measurement by talking to Android's sensor service through Binder without using the public `ASensorManager` wrapper.

Keep the observable action identical to the Android-API example:

```text
read one accelerometer measurement
write x y z in m/s^2
exit
```

This experiment exists to expose the Android userspace boundary and protocol cost. It must not be implemented by quietly calling `libandroid` and labelling that Binder. Record service-manager, Binder-driver, permission, and SELinux failures explicitly.

The read-only reconnaissance in [`../../probe-phone.sh`](../../probe-phone.sh) can establish which Binder nodes and sensor-related services are visible before a direct client is written.
