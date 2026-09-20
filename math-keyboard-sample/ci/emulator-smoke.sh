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
adb shell am start -W -n "$component" | tee /tmp/start.txt
grep -Fq 'Status: ok' /tmp/start.txt
sleep 2

adb shell uiautomator dump /sdcard/math-sample.xml
adb pull /sdcard/math-sample.xml /tmp/math-sample.xml
sed 's/></>\n</g' /tmp/math-sample.xml > /tmp/math-sample-nodes.xml

target_bounds=$(
    sed -n 's/.*content-desc="sample_target"[^>]*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p' \
        /tmp/math-sample-nodes.xml |
        head -n 1
)
test -n "$target_bounds"
set -- $target_bounds
x=$((($1 + $3) / 2))
y=$((($2 + $4) / 2))
adb shell input tap "$x" "$y"
sleep 2

adb shell uiautomator dump /sdcard/math-keyboard.xml
adb pull /sdcard/math-keyboard.xml /tmp/math-keyboard.xml
sed 's/></>\n</g' /tmp/math-keyboard.xml > /tmp/math-sample-nodes.xml
adb exec-out screencap -p > /tmp/math-keyboard-sample.png
printf '%s\n' 'UI NODES BEFORE KEY TAP'
sed -n '1,200p' /tmp/math-sample-nodes.xml

for symbol in ℕ ℤ ℚ ℝ ℂ = ≠ ≟ ∧ → λ π ∂ ∫ ∞ ⁿ ᵢ ² − –
do
    grep -Fq "text=\"$symbol\"" /tmp/math-sample-nodes.xml
done

lambda_bounds=$(
    sed -n 's/.*text="λ"[^>]*bounds="\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]\[\([0-9][0-9]*\),\([0-9][0-9]*\)\]".*/\1 \2 \3 \4/p' \
        /tmp/math-sample-nodes.xml |
        head -n 1
)
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
