#!/bin/sh
set -eu
apk=${1:?usage: check-apk.sh path/to.apk}
if [ ! -f "$apk" ]; then
    echo "missing APK: $apk" >&2
    exit 1
fi
sdk_root=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
build_tools="$sdk_root/build-tools/36.0.0"

"$build_tools/apksigner" verify --verbose "$apk"
"$build_tools/aapt2" dump badging "$apk" | grep -F "package: name='com.isomorphisms.pasteboard'"
"$build_tools/aapt2" dump badging "$apk" | grep -F "launchable-activity: name='android.app.NativeActivity'"
for abi in arm64-v8a armeabi-v7a x86_64; do
    unzip -l "$apk" "lib/$abi/libpasteboard.so" | grep -F "lib/$abi/libpasteboard.so"
done
"$build_tools/zipalign" -c -P 16 4 "$apk"
echo "pasteboard APK contract: all checks passed"
