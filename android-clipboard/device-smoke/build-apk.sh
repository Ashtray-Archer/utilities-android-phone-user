#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
bridge_root=$(CDPATH='' cd -- "$root/.." && pwd)
sdk_root=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
if [ -z "$sdk_root" ]; then
    echo "ANDROID_HOME or ANDROID_SDK_ROOT is required" >&2
    exit 2
fi

ndk_root=${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$sdk_root/ndk/27.2.12479018}}
toolchain="$ndk_root/toolchains/llvm/prebuilt/linux-x86_64"
build_tools="$sdk_root/build-tools/36.0.0"
platform_jar="$sdk_root/platforms/android-36/android.jar"
abi=${ANDROID_ABI:-armeabi-v7a}
api=${ANDROID_API:-24}

case "$abi" in
    armeabi-v7a) compiler="$toolchain/bin/armv7a-linux-androideabi${api}-clang" ;;
    arm64-v8a) compiler="$toolchain/bin/aarch64-linux-android${api}-clang" ;;
    x86_64) compiler="$toolchain/bin/x86_64-linux-android${api}-clang" ;;
    *)
        echo "unsupported ANDROID_ABI: $abi" >&2
        exit 2
        ;;
esac

for required in "$compiler" "$build_tools/aapt2" "$build_tools/zipalign" "$build_tools/apksigner" "$platform_jar"; do
    if [ ! -e "$required" ]; then
        echo "missing Android build dependency: $required" >&2
        exit 2
    fi
done

work=$(mktemp -d "${TMPDIR:-/tmp}/clipboard-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
objects="$work/objects"
staging="$work/staging"
output="$root/build/clipboard-smoke-$abi.apk"
mkdir -p "$objects" "$staging/lib/$abi" "$root/build"

make -f "$bridge_root/../icky/Android.mk" native \
    PROFILE=clipboard ABI="$abi" API="$api" NDK="$ndk_root" \
    BUILD="$objects" OUT="$staging/lib/$abi/libclipboard_smoke.so"

base_apk="$work/base.apk"
unsigned_apk="$work/unsigned.apk"
aligned_apk="$work/aligned.apk"
"$build_tools/aapt2" link -I "$platform_jar" \
    --manifest "$root/AndroidManifest.xml" \
    --min-sdk-version "$api" --target-sdk-version 36 \
    --version-code 1 --version-name 0.1.0 \
    -o "$base_apk"
cp "$base_apk" "$unsigned_apk"
(
    cd "$staging"
    zip -0 -q "$unsigned_apk" "lib/$abi/libclipboard_smoke.so"
)
"$build_tools/zipalign" -f -P 16 4 "$unsigned_apk" "$aligned_apk"

keystore=${ANDROID_KEYSTORE:?ANDROID_KEYSTORE must name the persistent public test signer}
keytool -exportcert -keystore "$keystore" -storetype PKCS12 \
    -storepass wegert-debug -alias wegert-debug |
    sha256sum |
    grep -qi '^de9b1d47c5a65e6d46a204b79dd9ee566b9d3c9832ba81ebc4213d3392e92ff9 '
"$build_tools/apksigner" sign \
    --ks "$keystore" --ks-key-alias wegert-debug \
    --ks-pass pass:wegert-debug --key-pass pass:wegert-debug \
    --out "$output" "$aligned_apk"
"$build_tools/apksigner" verify --verbose --print-certs "$output"

printf '%s\n' "$output"
