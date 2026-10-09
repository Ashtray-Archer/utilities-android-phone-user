#!/bin/sh
set -eu

ndk_root=${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}
api=${ANDROID_API:-21}
out_dir=${OUT_DIR:-out/android-native}

if [ -z "$ndk_root" ]; then
    echo "set ANDROID_NDK_HOME or ANDROID_NDK_ROOT" >&2
    exit 2
fi

host_tag=${ANDROID_NDK_HOST_TAG:-linux-x86_64}
toolchain="$ndk_root/toolchains/llvm/prebuilt/$host_tag/bin"
hardware_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$hardware_dir/.." && pwd)
source_file="$hardware_dir/android-native/hardware_sensor.c"
accelerometer_dir="$repository_dir/accelerometer/android"
accelerometer_model_dir="$repository_dir/accelerometer/model"
accelerometer_command_dir="$repository_dir/accelerometer/command"
accelerometer_source="$accelerometer_dir/android_accelerometer.c"

mkdir -p "$out_dir"

build_one() {
    name=$1
    abi=$2
    make -f "$repository_dir/icky/Android.mk" native \
        PROFILE=hardware ABI="$abi" API="$api" NDK="$ndk_root" \
        OUT="$(cd "$out_dir" && pwd)/hardware-android-native-$name"
}

build_one armv7 armeabi-v7a
build_one aarch64 arm64-v8a
build_one x86_64 x86_64

for binary in "$out_dir"/hardware-android-native-*; do
    printf '%s  ' "$(basename "$binary")"
    sha256sum "$binary" | awk '{print $1}'
done
