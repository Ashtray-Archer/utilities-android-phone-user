#!/bin/sh
set -eu
apk=${1:?usage: emulator-smoke.sh path/to.apk screenshot.png}
screenshot=${2:-/tmp/pasteboard-smoke.png}
package=com.isomorphisms.pasteboard
adb install -r "$apk"
adb logcat -c
adb shell am force-stop "$package"
adb shell monkey -p "$package" -c android.intent.category.LAUNCHER 1 >/dev/null
sleep 3
adb shell dumpsys activity activities | grep -F "$package"
if adb logcat -d '*:E' | grep -F 'FATAL EXCEPTION' | grep -F "$package"; then
    echo "pasteboard crashed" >&2
    exit 1
fi
adb exec-out screencap -p > "$screenshot"
test -s "$screenshot"
echo "pasteboard emulator smoke: launched"
