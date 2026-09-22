#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    printf '%s\n' "usage: emulator-smoke.sh APK" >&2
    exit 2
fi

apk=$1
package=com.ashtrayarcher.mathsamplekeyboard
component="$package/.MainActivity"
ime="$package/.MathSampleIme"

dump_nodes() {
    remote=$1
    local_file=$2
    adb shell uiautomator dump "$remote"
    adb pull "$remote" "$local_file"
    sed 's/></>\n</g' "$local_file" > /tmp/math-sample-nodes.xml
}

dismiss_system_ui_anr() {
    if ! grep -Fq 'resource-id="android:id/aerr_wait"' /tmp/math-sample-nodes.xml; then
        return 1
    fi

    wait_node=$(sed -n '/resource-id="android:id\/aerr_wait"/p' /tmp/math-sample-nodes.xml | head -n 1)
    wait_bounds=$(printf '%s\n' "$wait_node" | sed -n 's/.*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p')
    test -n "$wait_bounds"
    set -- $wait_bounds
    adb shell input tap "$((($1 + $3) / 2))" "$((($2 + $4) / 2))"
    return 0
}

adb install -r "$apk"
adb install -r "$apk"
adb shell ime enable "$ime"
adb shell ime set "$ime"
adb shell settings put secure show_ime_with_hard_keyboard 1
adb shell am start -W -n "$component" | tee /tmp/start.txt
grep -Fq 'Status: ok' /tmp/start.txt
sleep 2

dump_nodes /sdcard/math-sample.xml /tmp/math-sample.xml
if dismiss_system_ui_anr; then
    sleep 3
    dump_nodes /sdcard/math-sample.xml /tmp/math-sample.xml
fi
printf '%s\n' 'INITIAL UI NODES'
sed -n '1,200p' /tmp/math-sample-nodes.xml
grep -Fq 'content-desc="sample_target"' /tmp/math-sample-nodes.xml

root_node=$(sed -n '/class="android.widget.FrameLayout".*bounds=/p' /tmp/math-sample-nodes.xml | head -n 1)
root_bounds=$(printf '%s\n' "$root_node" | sed -n 's/.*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p')
content_node=$(sed -n '/resource-id="android:id\/content"/p' /tmp/math-sample-nodes.xml | head -n 1)
content_bounds=$(printf '%s\n' "$content_node" | sed -n 's/.*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p')
test -n "$root_bounds"
test -n "$content_bounds"

set -- $root_bounds
screen_width=$3
keyboard_bottom=$4
set -- $content_bounds
keyboard_top=$4
test "$keyboard_top" -lt "$keyboard_bottom"

# Confirm that the complete declared key vocabulary is present in the input view.
for symbol in ℕ ℤ ℚ ℝ ℂ = ≠ ≟ ∧ ← → λ π ∂ ∫ ∞ ⁿ ᵢ ² − –; do
    grep -Fq "text=\"$symbol\"" /tmp/math-sample-nodes.xml
done

# The input view has five equal rows. Left arrow is the fifth of six keys in
# row two; lambda is the first of five keys in row three.
left_arrow_x=$((screen_width * 3 / 4))
left_arrow_y=$((keyboard_top + (keyboard_bottom - keyboard_top) * 3 / 10))
lambda_x=$((screen_width / 10))
lambda_y=$((keyboard_top + (keyboard_bottom - keyboard_top) / 2))
adb exec-out screencap -p > /tmp/math-keyboard-sample.png
adb shell input tap "$left_arrow_x" "$left_arrow_y"
adb shell input tap "$lambda_x" "$lambda_y"
sleep 1

dump_nodes /sdcard/math-sample-after.xml /tmp/math-sample-after.xml
if dismiss_system_ui_anr; then
    sleep 3
    dump_nodes /sdcard/math-sample-after.xml /tmp/math-sample-after.xml
fi
grep -F 'class="android.widget.EditText"' /tmp/math-sample-nodes.xml |
    grep -F 'text="←λ"' |
    grep -Fq 'content-desc="sample_target"'

printf '%s\n' 'PASS all 21 character keys were visible and ← then λ typed ←λ'
