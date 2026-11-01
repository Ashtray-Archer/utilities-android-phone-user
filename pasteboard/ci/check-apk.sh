#!/bin/sh
set -eu
apk=${1:?usage: check-apk.sh path/to.apk}
if [ ! -f "$apk" ]; then
    echo "missing APK: $apk" >&2
    exit 1
fi
sdk_root=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
build_tools="$sdk_root/build-tools/36.0.0"

expected_signer_sha256=${ANDROID_EXPECTED_CERT_SHA256:-DE:9B:1D:47:C5:A6:5E:6D:46:A2:04:B7:9D:D9:EE:56:6B:9D:3C:98:32:BA:81:EB:C4:21:3D:33:92:E9:2F:F9}
"$build_tools/apksigner" verify --verbose --print-certs "$apk" > "${TMPDIR:-/tmp}/pasteboard-signing-$.txt"
trap 'rm -f "${TMPDIR:-/tmp}/pasteboard-signing-$.txt"' EXIT HUP INT TERM
grep -Fq "Signer #1 certificate SHA-256 digest: $(printf '%s' "$expected_signer_sha256" | tr '[:upper:]' '[:lower:]' | tr -d ':')" "${TMPDIR:-/tmp}/pasteboard-signing-$.txt"
"$build_tools/aapt2" dump badging "$apk" | grep -F "package: name='com.isomorphisms.pasteboard'"
"$build_tools/aapt2" dump badging "$apk" | grep -F "launchable-activity: name='android.app.NativeActivity'"
if unzip -Z1 "$apk" | grep -qx 'classes.dex'; then
    echo "Unexpected classes.dex in NativeActivity-only Pasteboard" >&2
    exit 1
fi
for abi in arm64-v8a armeabi-v7a x86_64; do
    unzip -l "$apk" "lib/$abi/libpasteboard.so" | grep -F "lib/$abi/libpasteboard.so"
done
"$build_tools/zipalign" -c -P 16 4 "$apk"
echo "pasteboard APK contract: all checks passed"
