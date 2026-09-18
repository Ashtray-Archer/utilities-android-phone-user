#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk_root=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
if [ -z "$sdk_root" ]; then
    echo "ANDROID_HOME or ANDROID_SDK_ROOT is required" >&2
    exit 2
fi

build_tools="$sdk_root/build-tools/36.0.0"
platform_jar="$sdk_root/platforms/android-36/android.jar"

for required in     "$build_tools/aapt2"     "$build_tools/d8"     "$build_tools/zipalign"     "$build_tools/apksigner"     "$platform_jar"
do
    if [ ! -e "$required" ]; then
        echo "missing Android build dependency: $required" >&2
        exit 2
    fi
done

for command_name in javac jar zip keytool; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "missing host build dependency: $command_name" >&2
        exit 2
    fi
done

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/sms-transport-build.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

classes_dir="$work_dir/classes"
dex_dir="$work_dir/dex"
staging_dir="$work_dir/staging"
output_dir="$project_dir/app/build/outputs/apk/debug"
mkdir -p "$classes_dir" "$dex_dir" "$staging_dir" "$output_dir"

source_list="$work_dir/java-sources.txt"
find "$project_dir/app/src/main/java" -type f -name '*.java' -print | sort >"$source_list"
if [ ! -s "$source_list" ]; then
    echo "no Java sources found" >&2
    exit 2
fi

javac     -encoding UTF-8     -source 8     -target 8     -classpath "$platform_jar"     -d "$classes_dir"     @"$source_list"

classes_jar="$work_dir/classes.jar"
jar cf "$classes_jar" -C "$classes_dir" .

"$build_tools/d8"     --lib "$platform_jar"     --min-api 26     --output "$dex_dir"     "$classes_jar"

base_apk="$work_dir/base.apk"
unsigned_apk="$work_dir/unsigned.apk"
aligned_apk="$work_dir/aligned.apk"
final_apk="$output_dir/sms-transport.apk"

"$build_tools/aapt2" link     -I "$platform_jar"     --manifest "$project_dir/app/src/main/AndroidManifest.xml"     --min-sdk-version 26     --target-sdk-version 36     --version-code 1     --version-name 0.1.0     -o "$base_apk"

cp "$base_apk" "$unsigned_apk"
cp "$dex_dir/classes.dex" "$staging_dir/classes.dex"
(
    cd "$staging_dir"
    zip -0 -q "$unsigned_apk" classes.dex
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
    keytool -list -v         -keystore "$keystore"         -storepass "$keystore_password"         -alias "$key_alias" 2>/dev/null |
        sed -n 's/^[[:space:]]*SHA256: //p' |
        head -n 1
)
if [ "$signer_sha256" != "$expected_signer_sha256" ]; then
    echo "unexpected Android test signer: ${signer_sha256:-missing}" >&2
    exit 2
fi

"$build_tools/apksigner" sign     --ks "$keystore"     --ks-key-alias "$key_alias"     --ks-pass "pass:$keystore_password"     --key-pass "pass:$key_password"     --out "$final_apk"     "$aligned_apk"

"$build_tools/apksigner" verify --verbose --print-certs "$final_apk" |
    tee "$work_dir/apk-signing.txt"
expected_compact=$(printf '%s' "$expected_signer_sha256" | tr '[:upper:]' '[:lower:]' | tr -d ':')
grep -Fq "Signer #1 certificate SHA-256 digest: $expected_compact" "$work_dir/apk-signing.txt"

echo "$final_apk"
