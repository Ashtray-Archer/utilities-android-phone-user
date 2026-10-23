#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Reuse the established native builder, ABI stripping and persistent signer.
exec sh "$project_dir/../accelerometer/build-apk.sh" spirit-level "$@"
