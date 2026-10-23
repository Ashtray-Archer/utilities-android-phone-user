#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo_dir=$(CDPATH= cd -- "$project_dir/.." && pwd)
receipts="$repo_dir/receipts"
mkdir -p "$receipts"

# CI emulator only. No uninstall, sensor accuracy claim, or phone ADB dependency.
apk="$project_dir/app/build/outputs/apk/distribution/spirit-level-x86_64.apk"
adb install -r -t "$apk"
adb install -r -t "$apk"
adb logcat -c
adb shell am start -W -n com.ashtrayarcher.spiritlevel/android.app.NativeActivity
attempt=0
while [ "$attempt" -lt 20 ]; do
    adb logcat -d -s SpiritLevel:I > "$receipts/emulator-log.txt"
    if grep -Fq 'Rendered spirit-level frame' "$receipts/emulator-log.txt"; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done
grep -Fq 'Rendered spirit-level frame' "$receipts/emulator-log.txt"
adb shell pidof com.ashtrayarcher.spiritlevel
adb exec-out screencap -p > "$receipts/emulator-screen.png"
printf '%s\n' \
    'PASS same-candidate replacement, launch and first native frame' \
    'NOT CLAIMED: older-build upgrade, physical sensor accuracy or phone execution' \
    > "$receipts/emulator.txt"
