#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec make -f "$project_dir/../icky/Host.mk" picker-tests
