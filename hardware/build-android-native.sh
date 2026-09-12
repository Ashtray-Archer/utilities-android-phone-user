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
source_file=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/android-native/hardware_sensor.c

mkdir -p "$out_dir"

build_one() {
    name=$1
    compiler=$2

    if [ ! -x "$toolchain/$compiler" ]; then
        echo "missing compiler: $toolchain/$compiler" >&2
        exit 2
    fi

    "$toolchain/$compiler" \
        -std=c17 \
        -O2 \
        -Wall \
        -Wextra \
        -Wpedantic \
        -Wno-deprecated-declarations \
        -fPIE \
        -pie \
        "$source_file" \
        -landroid \
        -o "$out_dir/hardware-android-native-$name"
}

build_one armv7 "armv7a-linux-androideabi${api}-clang"
build_one aarch64 "aarch64-linux-android${api}-clang"
build_one x86_64 "x86_64-linux-android${api}-clang"

for binary in "$out_dir"/hardware-android-native-*; do
    printf '%s  ' "$(basename "$binary")"
    sha256sum "$binary" | awk '{print $1}'
done
