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

grep -Fq 'android:hasCode="false"' "$manifest" ||
    fail 'SMS transport must not add an application DEX layer'

activity_block=$(
    awk '
        /android:name="android.app.NativeActivity"/ { in_activity = 1 }
        in_activity { print }
        in_activity && /<\/activity>/ { exit }
    ' "$manifest"
)

printf '%s\n' "$activity_block" | grep -Fq 'android:exported="true"' ||
    fail 'native SMS activity is not exported for the adb-shell test ingress'
printf '%s\n' "$activity_block" | grep -Fq 'android:permission="android.permission.DUMP"' ||
    fail 'native SMS activity must require android.permission.DUMP'
printf '%s\n' "$activity_block" | grep -Fq 'android:value="sms_transport"' ||
    fail 'native SMS activity is not bound to libsms_transport'

printf 'PASS manifest native outbound SMS boundary\n'
