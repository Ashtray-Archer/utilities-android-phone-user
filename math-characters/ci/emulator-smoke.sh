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

capture_failure_screen() {
    status=$?
    if [ "$status" -ne 0 ]; then
        failure_screen="$screenshot.failure.$$"
        if adb exec-out screencap -p > "$failure_screen" 2>/dev/null && [ -s "$failure_screen" ]; then
            mv "$failure_screen" "$screenshot"
        else
            rm -f "$failure_screen"
        fi
    fi
    trap - 0
    exit "$status"
}
trap capture_failure_screen 0

wait_for_process() {
    attempts=0
    app_pid=
    while [ "$attempts" -lt 20 ]; do
        app_pid=$(adb shell pidof "$package" 2>/dev/null | tr -d '\r' || true)
        if [ -n "$app_pid" ]; then
            return 0
        fi
        attempts=$((attempts + 1))
        sleep 1
    done
    echo "emulator-smoke: app process did not appear" >&2
    return 1
}

wait_for_focus() {
    attempts=0
    focus_state=
    while [ "$attempts" -lt 20 ]; do
        focus_state=$(adb shell dumpsys window 2>/dev/null | tr -d '\r' || true)
        if printf '%s\n' "$focus_state" |
            grep -E 'mCurrentFocus|mFocusedApp' |
            grep -F "$package" >/dev/null; then
            return 0
        fi
        attempts=$((attempts + 1))
        sleep 1
    done
    printf '%s\n' "$focus_state" |
        grep -E 'mCurrentFocus|mFocusedApp' >&2 || true
    echo "emulator-smoke: app window did not become focused" >&2
    return 1
}

wait_for_app_log() {
    needle=$1
    attempts=0
    state_log=
    while [ "$attempts" -lt 15 ]; do
        state_log=$(adb logcat -d --pid="$app_pid" -s ProgrammersUnicodePad:I '*:S' |
            tr -d '\r')
        if printf '%s\n' "$state_log" | grep -F "$needle" >/dev/null; then
            return 0
        fi
        attempts=$((attempts + 1))
        sleep 1
    done
    printf '%s\n' "$state_log" >&2
    echo "emulator-smoke: timed out waiting for app log: $needle" >&2
    return 1
}

adb install -r "$apk" >/dev/null
adb install -r "$apk" >/dev/null
adb logcat -c
adb shell am force-stop "$package"
adb shell am start -W -n "$component" | tee /tmp/programmers-unicode-pad-start.txt
grep -F 'Status: ok' /tmp/programmers-unicode-pad-start.txt >/dev/null

wait_for_process
wait_for_focus

physical_size=$(adb shell wm size | tr -d '\r' | sed -n 's/.*: \([0-9][0-9]*\)x\([0-9][0-9]*\).*/\1 \2/p' | tail -n 1)
set -- $physical_size
width=$1
height=$2

# Drive each state transition only after the preceding touch has reached the app.
adb shell input tap $((width / 14)) $((height * 38 / 100))
wait_for_app_log 'page=Unicode bytes=3'

adb shell input tap $((width * 11 / 12)) $((height * 25 / 100))

adb shell input tap $((width * 91 / 100)) $((height * 7 / 100))
wait_for_app_log 'page=Math bytes=3'

adb shell input tap $((width * 91 / 100)) $((height * 7 / 100))
wait_for_app_log 'page=Punctuation bytes=3'

adb shell input tap $((width / 8)) $((height * 38 / 100))
wait_for_app_log 'page=Punctuation bytes=6'

app_pid=$(adb shell pidof "$package" 2>/dev/null | tr -d '\r' || true)
test -n "$app_pid"

state_log=$(adb logcat -d --pid="$app_pid" -s ProgrammersUnicodePad:I '*:S')
printf '%s\n' "$state_log"

adb exec-out screencap -p > "$screenshot"
test "$(wc -c < "$screenshot")" -gt 10000

fatal_log=$(adb logcat -d --pid="$app_pid" \
    -s AndroidRuntime:E libc:F DEBUG:F ProgrammersUnicodePad:E '*:S')
if printf '%s\n' "$fatal_log" | grep -E 'FATAL EXCEPTION|Fatal signal|Abort message' >/dev/null; then
    printf '%s\n' "$fatal_log" >&2
    exit 1
fi

trap - 0
echo "emulator-smoke: punctuation page inserted Unicode minus and stayed alive"
