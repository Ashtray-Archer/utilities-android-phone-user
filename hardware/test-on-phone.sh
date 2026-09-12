#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

abi=""
if command -v getprop >/dev/null 2>&1; then
    abi=$(getprop ro.product.cpu.abi 2>/dev/null || true)
fi
if [ -z "$abi" ]; then
    abi=$(uname -m)
fi

case "$abi" in
    arm64-v8a|aarch64)
        binary="$script_dir/hardware-android-native-aarch64"
        ;;
    armeabi-v7a|armeabi|armv7l|armv8l|arm)
        binary="$script_dir/hardware-android-native-armv7"
        ;;
    x86_64)
        binary="$script_dir/hardware-android-native-x86_64"
        ;;
    *)
        echo "unsupported or unknown Android ABI: $abi" >&2
        exit 2
        ;;
esac

if [ ! -f "$binary" ]; then
    echo "missing binary for $abi: $binary" >&2
    exit 2
fi

chmod +x "$binary"

printf 'abi\t%s\n' "$abi"
printf 'binary\t%s\n' "$(basename "$binary")"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$binary"
fi

printf '\n== accelerometer info ==\n'
"$binary" read /hardware/sensors/accelerometer/info

printf '\n== one sample ==\n'
"$binary" read /hardware/sensors/accelerometer/sample

printf '\n== five timestamped events ==\n'
"$binary" read /hardware/sensors/accelerometer/events 5

if [ -f "$script_dir/probe-phone.sh" ]; then
    printf '\n== lower-layer visibility probe ==\n'
    sh "$script_dir/probe-phone.sh"
fi
