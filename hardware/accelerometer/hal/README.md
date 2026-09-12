# Accelerometer through the sensors HAL

Status: **NOT IMPLEMENTED / NOT CLAIMED ACCESSIBLE TO AN ORDINARY APP UID**.

Target experiment: obtain one accelerometer measurement by speaking to the device's actual sensors HAL boundary rather than going through `sensorservice` or `libandroid`.

Canonical observable behavior:

```text
read one accelerometer measurement
write x y z in m/s^2
exit
```

First identify whether this build uses the AIDL sensors HAL, a legacy HIDL interface, or vendor-specific machinery. Then record the exact service name/version and the service-manager/SELinux result from the same process context used for the other probes.

Do not count a Binder call to `sensorservice` as direct HAL access. Do not copy a HAL implementation into the client merely to make the example run.

Use [`../../probe-phone.sh`](../../probe-phone.sh) for initial service visibility evidence.
