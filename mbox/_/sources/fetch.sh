#!/bin/sh
set -eu

cd "$(dirname "$0")"

fetch() {
    url=$1
    out=$2
    tmp="${out}.tmp"
    rm -f "$tmp"
    curl -fL --retry 3 --retry-delay 1 "$url" -o "$tmp"
    mv "$tmp" "$out"
}

fetch https://www.rfc-editor.org/rfc/rfc4155.txt rfc4155.txt
fetch https://www.rfc-editor.org/rfc/rfc5322.txt rfc5322.txt
fetch https://www.rfc-editor.org/rfc/rfc2045.txt rfc2045.txt
fetch https://www.rfc-editor.org/rfc/rfc2046.txt rfc2046.txt

printf '%s\n' 'fetched RFC 4155, 5322, 2045, 2046'
