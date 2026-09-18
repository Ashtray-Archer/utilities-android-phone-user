#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
check="$project_dir/tests/check-manifest.sh"
good="$project_dir/app/src/main/AndroidManifest.xml"

sh "$check" "$good"

expect_failure() {
    fixture=$1
    expected=$2
    output=$(mktemp "${TMPDIR:-/tmp}/sms-transport-test.XXXXXX")
    if sh "$check" "$fixture" >"$output" 2>&1; then
        cat "$output" >&2
        rm -f "$output"
        printf 'FAIL hostile fixture unexpectedly passed: %s\n' "$fixture" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" "$output"; then
        cat "$output" >&2
        rm -f "$output"
        printf 'FAIL hostile fixture missed diagnostic: %s\n' "$expected" >&2
        exit 1
    fi
    rm -f "$output"
    printf 'PASS hostile fixture rejected: %s\n' "$expected"
}

expect_failure     "$project_dir/tests/fixtures/unprotected-send-activity.xml"     'send activity must require android.permission.DUMP'

expect_failure     "$project_dir/tests/fixtures/inbound-permission.xml"     'inbound SMS is outside this outbound-only slice'

send_source="$project_dir/app/src/main/java/com/ashtrayarcher/smstransport/SendActivity.java"
grep -Fq 'smsManager.sendTextMessage' "$send_source" || {
    printf 'FAIL sender does not call SmsManager.sendTextMessage\n' >&2
    exit 1
}

if find "$project_dir" -type f \( -name '*.gradle' -o -name '*.gradle.kts' -o -name '*.kt' \) -print | grep -q .; then
    printf 'FAIL Gradle/Kotlin files are outside this direct SDK build\n' >&2
    exit 1
fi

printf 'PASS direct Android SMS source contract\n'
