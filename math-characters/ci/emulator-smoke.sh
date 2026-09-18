#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 APK SCREENSHOT" >&2
    exit 2
fi

apk=$1
screenshot=$2
package=com.isomorphisms.programmersunicodepad
component=$package/android.app.NativeActivity

adb install -r "$apk" >/dev/null
adb install -r "$apk" >/dev/null
adb logcat -c
adb shell am force-stop "$package"
adb shell am start -W -n "$component" | tee /tmp/programmers-unicode-pad-start.txt
grep -F 'Status: ok' /tmp/programmers-unicode-pad-start.txt >/dev/null

sleep 2
test -n "$(adb shell pidof "$package" | tr -d '\r')"

physical_size=$(adb shell wm size | tr -d '\r' | sed -n 's/.*: \([0-9][0-9]*\)x\([0-9][0-9]*\).*/\1 \2/p' | tail -n 1)
set -- $physical_size
width=$1
height=$2

# Tap a Unicode arrow, COPY, advance through Math, and tap punctuation minus.
adb shell input tap $((width / 14)) $((height * 38 / 100))
adb shell input tap $((width * 11 / 12)) $((height * 25 / 100))
adb shell input tap $((width * 91 / 100)) $((height * 7 / 100))
adb shell input tap $((width * 91 / 100)) $((height * 7 / 100))
adb shell input tap $((width / 8)) $((height * 38 / 100))
sleep 1

app_pid=$(adb shell pidof "$package" | tr -d '\r')
test -n "$app_pid"
state_log=$(adb logcat -d --pid="$app_pid" -s ProgrammersUnicodePad:I '*:S')
printf '%s\n' "$state_log"
printf '%s\n' "$state_log" | grep -F 'page=Punctuation bytes=6' >/dev/null
adb exec-out screencap -p > "$screenshot"
test "$(wc -c < "$screenshot")" -gt 10000

fatal_log=$(adb logcat -d --pid="$app_pid" \
    -s AndroidRuntime:E libc:F DEBUG:F ProgrammersUnicodePad:E '*:S')
if printf '%s\n' "$fatal_log" | grep -E 'FATAL EXCEPTION|Fatal signal|Abort message' >/dev/null; then
    printf '%s\n' "$fatal_log" >&2
    exit 1
fi

echo "emulator-smoke: punctuation page inserted Unicode minus and stayed alive"
