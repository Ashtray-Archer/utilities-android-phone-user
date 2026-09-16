#!/bin/sh
set -eu

repository='Ashtray-Archer/utilities-android-phone-user'
artifact_name='phone-hardware-test'
api_root="https://api.github.com/repos/$repository"
install_root="${HOME}/.local/share/isomorphisms/phone-hardware-test"
temporary_root=$(mktemp -d "${TMPDIR:-/data/data/com.termux/files/usr/tmp}/phone-hardware-test.XXXXXX")

cleanup() {
    rm -rf "$temporary_root"
}
trap cleanup EXIT HUP INT TERM

if ! command -v curl >/dev/null 2>&1; then
    echo 'curl is required' >&2
    exit 2
fi
if ! command -v unzip >/dev/null 2>&1; then
    echo 'unzip is required; in Termux: pkg install unzip' >&2
    exit 2
fi

request() {
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        curl -fsSL \
            -H "Authorization: Bearer $GITHUB_TOKEN" \
            -H 'Accept: application/vnd.github+json' \
            -H 'X-GitHub-Api-Version: 2022-11-28' \
            "$@"
    else
        curl -fsSL \
            -H 'Accept: application/vnd.github+json' \
            -H 'X-GitHub-Api-Version: 2022-11-28' \
            "$@"
    fi
}

artifact_json="$temporary_root/artifacts.json"
request "$api_root/actions/artifacts?name=$artifact_name&per_page=10" > "$artifact_json"

artifact_id=$(grep -o '"id":[0-9][0-9]*' "$artifact_json" | head -n 1 | cut -d: -f2)
head_sha=$(grep -o '"head_sha":"[0-9a-f][0-9a-f]*"' "$artifact_json" | head -n 1 | cut -d'"' -f4)

if [ -z "$artifact_id" ]; then
    echo "no GitHub Actions artifact named $artifact_name was found" >&2
    exit 1
fi

artifact_zip="$temporary_root/$artifact_name.zip"
request -o "$artifact_zip" "$api_root/actions/artifacts/$artifact_id/zip"

rm -rf "$install_root"
mkdir -p "$install_root"
unzip -q "$artifact_zip" -d "$install_root"

printf 'repository\t%s\n' "$repository"
printf 'artifact\t%s\n' "$artifact_id"
if [ -n "$head_sha" ]; then
    printf 'head_sha\t%s\n' "$head_sha"
fi
printf 'installed\t%s\n' "$install_root"

receipt="$install_root/phone-hardware-receipt.txt"
sh "$install_root/test-on-phone.sh" 2>&1 | tee "$receipt"
printf '\nreceipt\t%s\n' "$receipt"
