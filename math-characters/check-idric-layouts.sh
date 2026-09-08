#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
compiler_name=${IDRIC_COMPILER:-idris2}
compiler=$(command -v "$compiler_name" 2>/dev/null || true)

if [ -z "$compiler" ]; then
    echo "Idriç compiler not found; set IDRIC_COMPILER" >&2
    exit 2
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/unicode-picker-idric.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

cd "$project_dir/idric"
"$compiler" --cg chez \
    --build-dir "$work_dir/generator-build" \
    --output-dir "$work_dir/generator-exec" \
    -o generate-layouts GenerateLayouts.idric
"$compiler" --cg chez \
    --build-dir "$work_dir/contract-build" \
    --output-dir "$work_dir/contract-exec" \
    -o picker-contract PickerContract.idric

"$work_dir/generator-exec/generate-layouts" >"$work_dir/pad_layouts.generated.inc"
checked_in="$project_dir/app/src/main/c/pad_layouts.generated.inc"
if ! cmp -s "$checked_in" "$work_dir/pad_layouts.generated.inc"; then
    echo "generated C layout snapshot does not match UnicodePicker.idric" >&2
    diff -u "$checked_in" "$work_dir/pad_layouts.generated.inc" || true
    exit 1
fi

contract_output=$("$work_dir/contract-exec/picker-contract")
if [ "$contract_output" != "picker_contract: all checks passed" ]; then
    printf '%s\n' "$contract_output" >&2
    exit 1
fi

refusal_dir="$work_dir/refusal"
mkdir -p "$refusal_dir"
ln -s "$project_dir/idric/UnicodePicker.idric" "$refusal_dir/UnicodePicker.idric"
ln -s "$project_dir/idric/rejected/OutOfRangePasteSlot.idric" \
    "$refusal_dir/OutOfRangePasteSlot.idric"
refusal_log="$refusal_dir/refusal.log"
if (cd "$refusal_dir" &&
    "$compiler" --check --build-dir "$refusal_dir/build" \
        OutOfRangePasteSlot.idric) >"$refusal_log" 2>&1; then
    echo "out-of-range paste slot unexpectedly compiled" >&2
    exit 1
fi
if ! grep -F "Mismatch between: Nat and paste_slot" "$refusal_log" >/dev/null; then
    echo "out-of-range paste slot failed for an unexpected reason" >&2
    sed -n '1,120p' "$refusal_log" >&2
    exit 1
fi

printf '%s\n' "$contract_output"
printf '%s\n' "paste_slot: out-of-range construction rejected"
