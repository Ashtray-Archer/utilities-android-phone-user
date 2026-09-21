#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk_root=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
if [ -z "$sdk_root" ]; then
    echo "ANDROID_HOME or ANDROID_SDK_ROOT is required" >&2
    exit 2
fi

ndk_root=${ANDROID_NDK_ROOT:-${ANDROID_NDK_HOME:-$sdk_root/ndk/27.2.12479018}}
toolchain="$ndk_root/toolchains/llvm/prebuilt/linux-x86_64"
glue_dir="$ndk_root/sources/android/native_app_glue"
build_tools="$sdk_root/build-tools/36.0.0"
platform_jar="$sdk_root/platforms/android-36/android.jar"

for required in \
    "$toolchain/bin/aarch64-linux-android26-clang" \
    "$toolchain/bin/armv7a-linux-androideabi26-clang" \
    "$toolchain/bin/x86_64-linux-android26-clang" \
    "$glue_dir/android_native_app_glue.c" \
    "$build_tools/aapt2" \
    "$build_tools/zipalign" \
    "$build_tools/apksigner" \
    "$platform_jar"
do
    if [ ! -e "$required" ]; then
        echo "missing Android build dependency: $required" >&2
        exit 2
    fi
done

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/accelerometer-build.XXXXXX")
staging_dir="$work_dir/staging"
output_dir="$project_dir/app/build/outputs/apk/debug"
mkdir -p "$staging_dir" "$output_dir"

compile_abi() {
    abi=$1
    compiler_name=$2
    architecture_flags=$3
    compiler="$toolchain/bin/$compiler_name"
    object_dir="$work_dir/objects/$abi"
    library_dir="$staging_dir/lib/$abi"
    mkdir -p "$object_dir" "$library_dir"

    common_flags="-std=c17 -O2 -g -fPIC -ffunction-sections -fdata-sections"
    warnings="-Wall -Wextra -Werror -Wpedantic -Wshadow"
    includes="-I$project_dir/app/src/main/c -isystem $glue_dir"

    # shellcheck disable=SC2086
    "$compiler" $common_flags $warnings $architecture_flags -fstack-protector-strong -D_FORTIFY_SOURCE=2 $includes -c "$project_dir/app/src/main/c/native_main.c" -o "$object_dir/native_main.o"
    # shellcheck disable=SC2086
    "$compiler" $common_flags $architecture_flags -isystem "$glue_dir" -c "$glue_dir/android_native_app_glue.c" -o "$object_dir/native_app_glue.o"

    # shellcheck disable=SC2086
    "$compiler" $architecture_flags -shared -Wl,--no-undefined -Wl,--gc-sections -Wl,-z,relro,-z,now -Wl,-u,ANativeActivity_onCreate "$object_dir/native_main.o" "$object_dir/native_app_glue.o" -landroid -llog -lm -o "$library_dir/libaccelerometer.so"
}

compile_abi arm64-v8a aarch64-linux-android26-clang ""
compile_abi armeabi-v7a armv7a-linux-androideabi26-clang "-mthumb -march=armv7-a"
compile_abi x86_64 x86_64-linux-android26-clang ""

base_apk="$work_dir/base.apk"
unsigned_apk="$work_dir/unsigned.apk"
aligned_apk="$work_dir/aligned.apk"
final_apk="$output_dir/app-debug.apk"
compiled_resources="$work_dir/compiled-resources.zip"

"$build_tools/aapt2" compile \
    --dir "$project_dir/app/src/main/res" \
    -o "$compiled_resources"

"$build_tools/aapt2" link \
    -I "$platform_jar" \
    --manifest "$project_dir/app/src/main/AndroidManifest.xml" \
    --min-sdk-version 26 \
    --target-sdk-version 36 \
    --version-code 1 \
    --version-name 0.1.0 \
    -o "$base_apk" \
    "$compiled_resources"
cp "$base_apk" "$unsigned_apk"
(
    cd "$staging_dir"
    zip -0 -q "$unsigned_apk" \
        lib/arm64-v8a/libaccelerometer.so \
        lib/armeabi-v7a/libaccelerometer.so \
        lib/x86_64/libaccelerometer.so
)

"$build_tools/zipalign" -f -P 16 4 "$unsigned_apk" "$aligned_apk"

keystore=${ANDROID_KEYSTORE:-}
keystore_password=${ANDROID_KEYSTORE_PASSWORD:-wegert-debug}
key_password=${ANDROID_KEY_PASSWORD:-$keystore_password}
key_alias=${ANDROID_KEY_ALIAS:-wegert-debug}
expected_signer_sha256=${ANDROID_EXPECTED_CERT_SHA256:-DE:9B:1D:47:C5:A6:5E:6D:46:A2:04:B7:9D:D9:EE:56:6B:9D:3C:98:32:BA:81:EB:C4:21:3D:33:92:E9:2F:F9}

if [ -z "$keystore" ]; then
    echo "ANDROID_KEYSTORE is required; refusing to generate a throwaway APK signer" >&2
    exit 2
fi
if [ ! -f "$keystore" ]; then
    echo "missing Android signing keystore: $keystore" >&2
    exit 2
fi

signer_sha256=$(
    keytool -list -v \
        -keystore "$keystore" \
        -storepass "$keystore_password" \
        -alias "$key_alias" 2>/dev/null |
        sed -n 's/^[[:space:]]*SHA256: //p' |
        head -n 1
)
if [ "$signer_sha256" != "$expected_signer_sha256" ]; then
    echo "unexpected Android test signer: ${signer_sha256:-missing}" >&2
    exit 2
fi

"$build_tools/apksigner" sign \
    --ks "$keystore" \
    --ks-key-alias "$key_alias" \
    --ks-pass "pass:$keystore_password" \
    --key-pass "pass:$key_password" \
    --out "$final_apk" \
    "$aligned_apk"

"$build_tools/apksigner" verify --verbose --print-certs "$final_apk" |
    tee "$work_dir/apk-signing.txt"
grep -Fq "Signer #1 certificate SHA-256 digest: $(printf '%s' "$expected_signer_sha256" | tr '[:upper:]' '[:lower:]' | tr -d ':')" "$work_dir/apk-signing.txt"

echo "$final_apk"
