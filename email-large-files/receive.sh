#!/usr/bin/env bash
# Bounded disk-based receiver. No file is published until its SHA-256 matches.
set -euo pipefail
if (( $# != 3 )); then
    printf 'Usage: %s URL EXPECTED_SHA256 DESTINATION\n' "${0##*/}" >&2
    exit 2
fi
url=$1
expected=$2
destination=$3
if [[ ! $expected =~ ^[0123456789abcdef]{64}$ ]]; then
    printf 'Expected SHA-256 must be 64 lowercase hex digits\n' >&2
    exit 2
fi
# A bearer token in a plain HTTP URL may only be sent over loopback.
case $url in
    https://*) ;;
    http://127.0.0.1:*|http://localhost:*) ;;
    *) printf 'Refusing non-HTTPS URL outside loopback\n' >&2; exit 2 ;;
esac
command -v curl >/dev/null || { printf 'curl is required\n' >&2; exit 2; }
command -v sha256sum >/dev/null || { printf 'sha256sum is required\n' >&2; exit 2; }
[[ ! -e $destination && ! -L $destination ]] || { printf 'Refusing to replace destination\n' >&2; exit 1; }
partial=$destination.elf-part
umask 077
[[ ! -L $partial ]] || { printf 'Refusing partial-file symlink\n' >&2; exit 1; }
[[ ! -e $partial || -f $partial ]] || { printf 'Partial path is not a regular file\n' >&2; exit 1; }
# Detect an already complete download left by an interrupted commit.
complete=false
if [[ -f $partial ]]; then
    read -r hash _ < <(sha256sum -- "$partial")
    [[ $hash != "$expected" ]] || complete=true
fi
if [[ $complete == false ]]; then
    curl --proto '=https,http' --fail --silent --show-error \
         --retry 2 --continue-at - --output "$partial" -- "$url"
    read -r hash _ < <(sha256sum -- "$partial")
    if [[ $hash != "$expected" ]]; then
        printf 'SHA-256 mismatch; unverified partial retained: %s\n' "$partial" >&2
        exit 1
    fi
fi
# Hard-link publication is atomic and refuses to overwrite an existing name.
ln -- "$partial" "$destination"
rm -- "$partial"
printf 'Verified SHA-256 %s\nSaved to %s\n' "$expected" "$destination"
