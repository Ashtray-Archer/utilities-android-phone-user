#!/bin/sh
set -eu

apk=${1:?usage: test-physical.sh APK DESTINATION}
destination=${2:-}

if [ -z "$destination" ]; then
    printf 'Destination telephone number: ' >&2
    IFS= read -r destination
fi
[ -n "$destination" ] || {
    printf 'destination telephone number is required\n' >&2
    exit 2
}

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
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

section() { printf '\n%b== %s ==%b\n' "$cyan" "$*" "$reset"; }
action() { printf '%bACTION%b %s\n' "$yellow" "$reset" "$*"; }
pass() { printf '%bPASS%b %s\n' "$green" "$reset" "$*"; }
fail() { printf '%bFAIL%b %s\n' "$red" "$reset" "$*" >&2; exit 1; }
note() { printf '%bNOTE%b %s\n' "$yellow" "$reset" "$*"; }

command -v adb >/dev/null 2>&1 || fail 'adb is not installed on this controlling device'
[ -f "$apk" ] || fail "APK does not exist: $apk"

package=com.ashtrayarcher.smstransport
component="$package/.SendActivity"
request_id="physical_$(date -u +%Y%m%dT%H%M%SZ)"

section 'device'
adb get-state >/dev/null 2>&1 || fail 'no adb device is connected'
adb shell getprop ro.product.model | tr -d '\r'
adb shell getprop ro.product.cpu.abi | tr -d '\r'

section 'replacement install and permission'
action 'replacement-installing exact APK and granting requested runtime permissions'
adb install -r -g "$apk"

permission_line=$(
    adb shell dumpsys package "$package" |
        tr -d '\r' |
        grep -F 'android.permission.SEND_SMS:' |
        head -n 1 || true
)
case "$permission_line" in
    *granted=true*) pass 'SEND_SMS is granted on the device' ;;
    *) fail "SEND_SMS is not granted; observed: ${permission_line:-no permission state}" ;;
esac

section 'submit exact SMS'
adb logcat -c
action 'submitting exact body "hey" through the protected adb-shell ingress'
adb shell am start -W     -n "$component"     --es destination "$destination"     --es body hey     --es request_id "$request_id" >/dev/null

submitted=false
sent=false
for _attempt in 1 2 3 4 5 6 7 8 9 10; do
    logs=$(adb logcat -d -s SmsTransport:I SmsTransport:E SmsTransport:W '*:S' | tr -d '\r')
    if printf '%s\n' "$logs" | grep -F "FAIL request_id=$request_id" >/dev/null; then
        printf '%s\n' "$logs" | grep -F "$request_id" >&2 || true
        fail 'Android rejected the SMS request'
    fi
    if printf '%s\n' "$logs" | grep -F "FAIL sent request_id=$request_id" >/dev/null; then
        printf '%s\n' "$logs" | grep -F "$request_id" >&2 || true
        fail 'Android reported that the SMS send failed'
    fi
    if printf '%s\n' "$logs" | grep -F "PASS request_submitted request_id=$request_id" >/dev/null; then
        submitted=true
    fi
    if printf '%s\n' "$logs" | grep -F "PASS sent request_id=$request_id" >/dev/null; then
        sent=true
        break
    fi
    sleep 1
done

[ "$submitted" = true ] || fail 'no request-submitted receipt appeared in SmsTransport log'
pass "SmsManager accepted the request: $request_id"

if [ "$sent" = true ]; then
    pass 'Android modem reported the SMS as sent'
else
    note 'no Android sent-result arrived during the short observation window'
fi

section 'physical acceptance boundary'
note 'Check the destination handset for the exact text: hey'
note "Carrier end-to-end acceptance is not complete until that observation is recorded for request $request_id"
