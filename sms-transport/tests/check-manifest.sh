#!/bin/sh
set -eu

manifest=${1:?usage: check-manifest.sh MANIFEST}

fail() {
    printf 'FAIL %s\n' "$*" >&2
    exit 1
}

grep -Fq 'android.permission.SEND_SMS' "$manifest" ||
    fail 'manifest does not request android.permission.SEND_SMS'

if grep -Fq 'android.permission.RECEIVE_SMS' "$manifest"; then
    fail 'inbound SMS is outside this outbound-only slice'
fi

send_block=$(
    awk '
        /android:name="\.SendActivity"/ { in_send = 1 }
        in_send { print }
        in_send && /<\/activity>/ { exit }
    ' "$manifest"
)

printf '%s\n' "$send_block" | grep -Fq 'android:exported="true"' ||
    fail 'send activity is not exported for the adb-shell test ingress'
printf '%s\n' "$send_block" | grep -Fq 'android:permission="android.permission.DUMP"' ||
    fail 'send activity must require android.permission.DUMP'

printf 'PASS manifest outbound SMS boundary\n'
