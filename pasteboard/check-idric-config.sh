#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
compiler_name=${IDRIC_COMPILER:-idris2}
compiler=$(command -v "$compiler_name" 2>/dev/null || true)
if [ -z "$compiler" ]; then
    echo "Idriç compiler not found; set IDRIC_COMPILER" >&2
    exit 2
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/pasteboard-idric.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM
cd "$project_dir/idric"

"$compiler" --cg chez \
    --build-dir "$work_dir/generator-build" \
    --output-dir "$work_dir/generator-exec" \
    -o generate-config GenerateConfig.idric
"$compiler" --cg chez \
    --build-dir "$work_dir/contract-build" \
    --output-dir "$work_dir/contract-exec" \
    -o pasteboard-contract PasteboardContract.idric

"$work_dir/generator-exec/generate-config" > "$work_dir/pasteboard_config.generated.h"
checked_in="$project_dir/app/src/main/c/pasteboard_config.generated.h"
if ! cmp -s "$checked_in" "$work_dir/pasteboard_config.generated.h"; then
    echo "generated pasteboard C config does not match Pasteboard.idric" >&2
    diff -u "$checked_in" "$work_dir/pasteboard_config.generated.h" || true
    exit 1
fi

contract_output=$("$work_dir/contract-exec/pasteboard-contract")
if [ "$contract_output" != "pasteboard_contract: all checks passed" ]; then
    printf '%s\n' "$contract_output" >&2
    exit 1
fi
printf '%s\n' "$contract_output"
