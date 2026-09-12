# Accelerometer through a Linux/kernel device interface

Status: **NOT IMPLEMENTED / NO SENSOR DEVICE NODE CLAIMED**.

Target experiment: read one accelerometer measurement from whatever kernel-facing interface this phone actually exposes to its Android userspace, if any: IIO, input, sysfs, a character device, or a vendor node.

Canonical observable behavior remains:

```text
read one accelerometer measurement
write x y z in m/s^2
exit
```

Do not infer a usable device from a directory name. Identify the concrete node, driver/interface, raw units and scale, axis convention, access mode, and SELinux result. A permission denial is boundary evidence, not a sensor reading.

Start with [`../../probe-phone.sh`](../../probe-phone.sh); it inspects likely IIO/input paths without opening arbitrary binary device nodes.
