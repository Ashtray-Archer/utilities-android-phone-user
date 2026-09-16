#!/bin/sh
set -eu

repository='Ashtray-Archer/utilities-android-phone-user'
workflow='android-hardware-native.yml'
branch='hardware-sensor-namespace'
artifact_name='phone-hardware-test'
install_root="${HOME}/.local/share/isomorphisms/phone-hardware-test"
temporary_root=$(mktemp -d "${TMPDIR:-/data/data/com.termux/files/usr/tmp}/phone-hardware-test.XXXXXX")

cleanup() {
    rm -rf "$temporary_root"
}
trap cleanup EXIT HUP INT TERM

if ! command -v gh >/dev/null 2>&1; then
    echo 'GitHub CLI is required; in Termux: pkg install gh' >&2
    exit 2
fi
if ! command -v unzip >/dev/null 2>&1; then
    echo 'unzip is required; in Termux: pkg install unzip' >&2
    exit 2
fi
if ! gh auth status -h github.com >/dev/null 2>&1; then
    echo 'GitHub CLI is not authenticated.' >&2
    echo 'Run: gh auth login -h github.com -p https -w' >&2
    exit 2
fi

run_record=$(
    gh run list \
        -R "$repository" \
        -w "$workflow" \
        -b "$branch" \
        -s success \
        -L 20 \
        --json databaseId,headSha,createdAt \
        --jq '.[0] | [.databaseId, .headSha, .createdAt] | @tsv'
)

if [ -z "$run_record" ]; then
    echo "no successful $workflow run was found on $branch" >&2
    exit 1
fi

old_ifs=$IFS
IFS=$(printf '\t')
set -- $run_record
IFS=$old_ifs
run_id=$1
head_sha=$2
created_at=$3

artifact_root="$temporary_root/artifact"
mkdir -p "$artifact_root"

gh run download "$run_id" \
    -R "$repository" \
    -n "$artifact_name" \
    -D "$artifact_root"

if [ ! -f "$artifact_root/test-on-phone.sh" ]; then
    echo "run $run_id did not contain the expected $artifact_name payload" >&2
    exit 1
fi

rm -rf "$install_root"
mkdir -p "$(dirname "$install_root")"
mv "$artifact_root" "$install_root"

printf 'repository\t%s\n' "$repository"
printf 'workflow\t%s\n' "$workflow"
printf 'run_id\t%s\n' "$run_id"
printf 'head_sha\t%s\n' "$head_sha"
printf 'created_at\t%s\n' "$created_at"
printf 'installed\t%s\n' "$install_root"

receipt="$install_root/phone-hardware-receipt.txt"
sh "$install_root/test-on-phone.sh" 2>&1 | tee "$receipt"
printf '\nreceipt\t%s\n' "$receipt"
