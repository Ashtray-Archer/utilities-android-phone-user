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

expect_failure     "$project_dir/tests/fixtures/unprotected-send-activity.xml"     'native SMS activity must require android.permission.DUMP'

expect_failure     "$project_dir/tests/fixtures/inbound-permission.xml"     'inbound SMS is outside this outbound-only slice'

source_file="$project_dir/app/src/main/c/native_sms_transport.c"
grep -Fq '"android/telephony/SmsManager"' "$source_file" || {
    printf 'FAIL native sender does not bind Android SmsManager through JNI\n' >&2
    exit 1
}
grep -Fq '"sendTextMessage"' "$source_file" || {
    printf 'FAIL native sender does not invoke SmsManager.sendTextMessage\n' >&2
    exit 1
}

if find "$project_dir" -type f \(     -name '*.java' -o     -name '*.kt' -o     -name '*.gradle' -o     -name '*.gradle.kts' \) -print | grep -q .; then
    printf 'FAIL Java/Kotlin/Gradle files are outside this native Android transport\n' >&2
    exit 1
fi

printf 'PASS native JNI Android SMS source contract\n'
