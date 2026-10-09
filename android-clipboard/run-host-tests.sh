#!/bin/sh
set -eu

root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
exec make -f "$root/../icky/Host.mk" clipboard-tests
