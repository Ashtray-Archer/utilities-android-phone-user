#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
adb=${ADB:-adb}
package=com.isomorphisms.clipboardsmoke
component="$package/android.app.NativeActivity"
apk=${1:-}

if [ -z "$apk" ]; then
    apk=$(sh "$root/build-apk.sh")
fi

"$adb" get-state >/dev/null
"$adb" uninstall "$package" >/dev/null 2>&1 || true
"$adb" logcat -c
"$adb" install "$apk" >/dev/null
start_output=$("$adb" shell am start -W -n "$component" 2>&1) || {
    printf '%s\n' "$start_output"
    "$adb" logcat -d
    exit 1
}
printf '%s\n' "$start_output"

receipt=''
attempt=0
while [ "$attempt" -lt 10 ]; do
    receipt=$("$adb" logcat -d -s ClipboardSmoke:I '*:S')
    if printf '%s\n' "$receipt" | grep -E 'CLIPBOARD_SMOKE (PASS|FAIL)' >/dev/null; then
        break
    fi
    attempt=$((attempt + 1))
    sleep 1
done

printf '%s\n' "$receipt"
if ! printf '%s\n' "$receipt" | grep -F 'CLIPBOARD_SMOKE PASS' >/dev/null; then
    printf '%s\n' 'FAIL: no clipboard smoke PASS receipt; relevant Android log follows' >&2
    "$adb" logcat -d | grep -E 'ClipboardSmoke|clipboardsmoke|NativeActivity|AndroidRuntime|linker|libclipboard_smoke' || true
    exit 1
fi
printf '%s\n' 'PASS: foreground Android ClipboardManager smoke receipt'
