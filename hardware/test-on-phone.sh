#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ -t 1 ] && [ "${TERM:-dumb}" != dumb ]; then
    cyan='\033[1;36m'
    yellow='\033[1;33m'
    green='\033[1;32m'
    red='\033[1;31m'
    reset='\033[0m'
else
    cyan=''
    yellow=''
    green=''
    red=''
    reset=''
fi

section() {
    printf '\n%b== %s ==%b\n' "$cyan" "$1" "$reset"
}

action() {
    printf '%b%s%b\n' "$yellow" "$1" "$reset"
}

fail() {
    printf '%bFAIL%b %s\n' "$red" "$reset" "$1" >&2
    exit 1
}

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
        fail "unsupported or unknown Android ABI: $abi"
        ;;
esac

if [ ! -f "$binary" ]; then
    fail "missing binary for $abi: $binary"
fi

chmod +x "$binary"

printf 'abi\t%s\n' "$abi"
printf 'binary\t%s\n' "$(basename "$binary")"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$binary"
fi

section "accelerometer info"
"$binary" read /hardware/sensors/accelerometer/info

section "one Android accelerometer reading"
"$binary" read /hardware/sensors/accelerometer/sample

section "complete semantic inspection"
"$binary" read /hardware/sensors/accelerometer/inspect

section "five timestamped Android readings"
"$binary" read /hardware/sensors/accelerometer/events 5

if [ -f "$script_dir/probe-phone.sh" ]; then
    section "lower-layer visibility probe"
    action "Checking which lower Android/kernel sensor layers are visible from this process"
    sh "$script_dir/probe-phone.sh"
fi

printf '\n%bPASS%b shared Android accelerometer reading and inspection path executed\n' "$green" "$reset"
