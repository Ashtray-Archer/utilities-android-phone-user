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

adb install -r "$apk"
adb install -r "$apk"
adb shell ime enable "$ime"
adb shell ime set "$ime"
adb shell settings put secure show_ime_with_hard_keyboard 1
adb shell am start -W -n "$component" | tee /tmp/start.txt
grep -Fq 'Status: ok' /tmp/start.txt
sleep 2

adb shell uiautomator dump /sdcard/math-sample.xml
adb pull /sdcard/math-sample.xml /tmp/math-sample.xml
sed 's/></>\n</g' /tmp/math-sample.xml > /tmp/math-sample-nodes.xml
printf '%s\n' 'INITIAL UI NODES'
sed -n '1,200p' /tmp/math-sample-nodes.xml

if grep -Fq 'text="λ"' /tmp/math-sample-nodes.xml; then
    printf '%s\n' 'Keyboard was already visible after activity launch'
else
    target_node=$(sed -n '/content-desc="sample_target"/p' /tmp/math-sample-nodes.xml | head -n 1)
    target_bounds=$(printf '%s\n' "$target_node" | sed -n 's/.*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p')
    test -n "$target_bounds"
    set -- $target_bounds
    x=$((($1 + $3) / 2))
    y=$((($2 + $4) / 2))
    adb shell input tap "$x" "$y"
    sleep 2

    adb shell uiautomator dump /sdcard/math-keyboard.xml
    adb pull /sdcard/math-keyboard.xml /tmp/math-keyboard.xml
    sed 's/></>\n</g' /tmp/math-keyboard.xml > /tmp/math-sample-nodes.xml
fi
adb exec-out screencap -p > /tmp/math-keyboard-sample.png
printf '%s\n' 'UI NODES BEFORE KEY TAP'
sed -n '1,200p' /tmp/math-sample-nodes.xml

for symbol in ℕ ℤ ℚ ℝ ℂ = ≠ ≟ ∧ → λ π ∂ ∫ ∞ ⁿ ᵢ ² − –
do
    grep -Fq "text=\"$symbol\"" /tmp/math-sample-nodes.xml
done

lambda_node=$(sed -n '/text="λ"/p' /tmp/math-sample-nodes.xml | head -n 1)
lambda_bounds=$(printf '%s\n' "$lambda_node" | sed -n 's/.*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p')
test -n "$lambda_bounds"
set -- $lambda_bounds
x=$((($1 + $3) / 2))
y=$((($2 + $4) / 2))
adb shell input tap "$x" "$y"
sleep 1

adb shell uiautomator dump /sdcard/math-sample-after.xml
adb pull /sdcard/math-sample-after.xml /tmp/math-sample-after.xml
sed 's/></>\n</g' /tmp/math-sample-after.xml > /tmp/math-sample-after-nodes.xml
grep -F 'class="android.widget.EditText"' /tmp/math-sample-after-nodes.xml |
    grep -F 'text="λ"' |
    grep -Fq 'content-desc="sample_target"'

printf '%s\n' 'PASS keyboard surfaced all 20 compact keys and typed λ'
