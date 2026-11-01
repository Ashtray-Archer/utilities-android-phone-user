#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/pasteboard-host.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM
cc -std=c17 -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -pedantic \
  -I"$project_dir/app/src/main/c" \
  "$project_dir/app/src/main/c/pasteboard_model.c" \
  "$project_dir/app/src/test-c/pasteboard_model_test.c" \
  -o "$work_dir/pasteboard_model_test"
"$work_dir/pasteboard_model_test"
