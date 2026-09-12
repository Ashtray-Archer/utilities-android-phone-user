#!/bin/sh
set -u

section() {
    printf '\n== %s ==\n' "$1"
}

run() {
    printf '$'
    for argument in "$@"; do
        printf ' %s' "$argument"
    done
    printf '\n'
    "$@" 2>&1 || true
}

show_path() {
    path=$1
    printf '\n-- %s --\n' "$path"
    if [ -e "$path" ]; then
        run ls -ld "$path"
        if command -v ls >/dev/null 2>&1; then
            ls -ldZ "$path" 2>/dev/null || true
        fi
        [ -r "$path" ] && echo "shell test: readable" || echo "shell test: not readable"
        [ -w "$path" ] && echo "shell test: writable" || echo "shell test: not writable"
    else
        echo "absent"
    fi
}

section "process and Android build"
run uname -a
run id
id -Z 2>&1 || true
if command -v getenforce >/dev/null 2>&1; then
    run getenforce
fi
if command -v getprop >/dev/null 2>&1; then
    run getprop ro.build.version.release
    run getprop ro.build.version.sdk
    run getprop ro.product.cpu.abi
    run getprop ro.product.cpu.abilist
fi

section "Binder device nodes"
show_path /dev/binder
show_path /dev/hwbinder
show_path /dev/vndbinder

section "sensor-related services visible to this process"
if command -v service >/dev/null 2>&1; then
    service list 2>&1 | grep -Ei 'sensor|hardware' || true
else
    echo "service command unavailable"
fi
if command -v cmd >/dev/null 2>&1; then
    cmd -l 2>&1 | grep -Ei 'sensor|hardware' || true
else
    echo "cmd command unavailable"
fi
if command -v lshal >/dev/null 2>&1; then
    lshal 2>&1 | grep -Ei 'sensor' || true
else
    echo "lshal unavailable"
fi

section "sensorservice diagnostic boundary"
if command -v dumpsys >/dev/null 2>&1; then
    dumpsys sensorservice 2>&1 | sed -n '1,100p'
else
    echo "dumpsys unavailable"
fi

section "kernel-facing sensor candidates"
show_path /dev/input
show_path /sys/class/input
show_path /sys/bus/iio/devices

if [ -d /sys/bus/iio/devices ]; then
    for device in /sys/bus/iio/devices/iio:device*; do
        [ -e "$device" ] || continue
        printf '\n-- %s --\n' "$device"
        ls -ldZ "$device" 2>/dev/null || ls -ld "$device" 2>&1 || true
        if [ -r "$device/name" ]; then
            printf 'name: '
            cat "$device/name" 2>&1 || true
        else
            echo "name: unreadable"
        fi
    done
fi

if [ -d /sys/class/input ]; then
    for device in /sys/class/input/input*; do
        [ -e "$device" ] || continue
        if [ -r "$device/name" ]; then
            printf '%s\t' "$device"
            cat "$device/name" 2>&1 || true
        fi
    done
fi

section "raw bus/register candidates"
show_path /dev/i2c-0
show_path /dev/spidev0.0
show_path /dev/mem

echo
printf '%s\n' "This probe only inspects visibility and ordinary permission checks."
printf '%s\n' "It does not claim that a visible Binder/HAL/kernel node is usable until a real sensor transaction succeeds."
