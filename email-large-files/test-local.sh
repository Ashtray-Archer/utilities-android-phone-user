#!/usr/bin/env bash
set -euo pipefail
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
tmp=$(mktemp -d)
pid=
cleanup() { [[ -z $pid ]] || kill "$pid" 2>/dev/null || :; rm -rf -- "$tmp"; }
trap cleanup EXIT
cc -std=c11 -D_FILE_OFFSET_BITS=64 -O2 -Wall -Wextra -Werror -pedantic \
    "$script_dir/serve.c" -o "$tmp/server"
printf 'The quick brown fox jumped over the lazy dog.\n' > "$tmp/message.txt"
"$tmp/server" "$tmp/message.txt" 0 60 > "$tmp/offer" 2> "$tmp/stderr" &
pid=$!
for _ in $(seq 1 100); do [[ -s "$tmp/offer" ]] && break; sleep .02; done
[[ -s "$tmp/offer" ]] || { cat "$tmp/stderr" >&2; exit 1; }
url=$(awk '$1 == "URL" {print $2}' "$tmp/offer")
[[ $url == http://127.0.0.1:* ]] || exit 1
curl -fsS --max-time 5 "$url" -o "$tmp/full"
cmp "$tmp/message.txt" "$tmp/full"
curl -fsSI --max-time 5 "$url" | grep -qi '^Accept-Ranges: bytes'
curl -fsS --max-time 5 -D "$tmp/headers" -H 'Range: bytes=4-8' "$url" -o "$tmp/part"
[[ $(wc -c < "$tmp/part") -eq 5 ]]
grep -q '^Content-Range: bytes 4-8/46' "$tmp/headers"
dd if="$tmp/message.txt" bs=1 skip=4 count=5 status=none | cmp - "$tmp/part"
curl -fsS --max-time 5 -H 'Range: bytes=-4' "$url" -o "$tmp/suffix"
tail -c 4 "$tmp/message.txt" | cmp - "$tmp/suffix"
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' -H 'Range: bytes=1000000-' "$url") == 416 ]]
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' -H 'Range: bytes=184467440737095516161-' "$url") == 416 ]]
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' -H 'Range: bytes=0-1,10-20' "$url") == 416 ]]
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' "${url}bad") == 404 ]]
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' -X DELETE "$url") == 405 ]]
expected=$(sha256sum "$tmp/message.txt")
expected=${expected%% *}
head -c 13 "$tmp/message.txt" > "$tmp/resumed.txt.elf-part"
"$script_dir/receive.sh" "$url" "$expected" "$tmp/resumed.txt" > "$tmp/receive-receipt"
cmp "$tmp/message.txt" "$tmp/resumed.txt"
[[ ! -e "$tmp/resumed.txt.elf-part" ]]
if "$script_dir/receive.sh" "$url" "$expected" "$tmp/resumed.txt" > /dev/null 2>&1; then
    echo 'Unexpected overwrite' >&2; exit 1
fi
if "$script_dir/receive.sh" "$url" "$(printf '%064d' 1)" "$tmp/invalid.txt" > /dev/null 2>&1; then
    echo 'Unexpected successful checksum' >&2; exit 1
fi
[[ ! -e "$tmp/invalid.txt" && -e "$tmp/invalid.txt.elf-part" ]]
if "$script_dir/receive.sh" 'http://example.com/file' "$expected" "$tmp/leak.txt" > /dev/null 2>&1; then
    echo 'Unexpected plaintext non-loopback accepted' >&2; exit 1
fi
printf extra >> "$tmp/message.txt"
[[ $(curl -s --max-time 5 -o /dev/null -w '%{http_code}' "$url") == 409 ]]
kill "$pid" 2>/dev/null || :
wait "$pid" 2>/dev/null || :
pid=
truncate -s 4294967312 "$tmp/large.bin"
"$tmp/server" "$tmp/large.bin" 0 60 > "$tmp/large-offer" 2> "$tmp/large-stderr" &
pid=$!
for _ in $(seq 1 100); do [[ -s "$tmp/large-offer" ]] && break; sleep .02; done
largeurl=$(awk '$1 == "URL" {print $2}' "$tmp/large-offer")
curl -fsS --max-time 5 -D "$tmp/large-headers" -H 'Range: bytes=4294967296-4294967301' "$largeurl" -o "$tmp/tail"
[[ $(wc -c < "$tmp/tail") -eq 6 ]]
grep -q '^Content-Range: bytes 4294967296-4294967301/4294967312' "$tmp/large-headers"
[[ $(od -An -tx1 "$tmp/tail" | tr -d ' \n') == 000000000000 ]]
echo 'PASS: full download, HEAD, byte ranges, errors, source mutation, >4 GiB offsets'
