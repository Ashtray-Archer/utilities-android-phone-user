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

# The input view has five equal rows. Lambda is the first key in row three.
x=$((screen_width / 10))
y=$((keyboard_top + (keyboard_bottom - keyboard_top) / 2))
adb exec-out screencap -p > /tmp/math-keyboard-sample.png
adb shell input tap "$x" "$y"
sleep 1

adb shell uiautomator dump /sdcard/math-sample-after.xml
adb pull /sdcard/math-sample-after.xml /tmp/math-sample-after.xml
sed 's/></>\n</g' /tmp/math-sample-after.xml > /tmp/math-sample-after-nodes.xml
grep -F 'class="android.widget.EditText"' /tmp/math-sample-after-nodes.xml |
    grep -F 'text="λ"' |
    grep -Fq 'content-desc="sample_target"'

printf '%s\n' 'PASS keyboard occupied the input area and its λ key typed λ'
