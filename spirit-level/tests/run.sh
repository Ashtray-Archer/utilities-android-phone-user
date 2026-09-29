#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
repo_dir=$(CDPATH= cd -- "$project_dir/.." && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/spirit-level-tests.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

# Explicit host build; this is not a phone installer or an Android sensor oracle.
# CFLAGS is intentionally split to accept additional compiler/sanitizer switches.
# shellcheck disable=SC2086
"${CC:-cc}" -std=c17 -O2 -Wall -Wextra -Werror -Wpedantic -Wshadow ${CFLAGS:-} \
    -I"$repo_dir/accelerometer/model" -I"$project_dir/model" \
    -I"$project_dir/app/src/main/c" "$project_dir/tests/spirit_level_test.c" \
    -lm -o "$work_dir/spirit-level-test"
"$work_dir/spirit-level-test"

# Project-specific ownership guard: the new consumer has no sensor acquisition
# implementation or fused/gyroscope/magnetometer path hidden in its own code.
if grep -REn 'ASensor|ASENSOR_TYPE_|SensorManager|SensorEventListener' \
    "$project_dir/app/src/main/c" "$project_dir/model"; then
    echo 'FAIL: spirit-level must use the shared Android accelerometer adapter' >&2
    exit 1
fi
native="$project_dir/app/src/main/c/native_main.c"
for action in open enable disable close next; do
    grep -Fq "android_accelerometer_$action(" "$native"
done
printf '%s\n' 'PASS shared-acquisition source boundary'
