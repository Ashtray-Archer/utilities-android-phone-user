#!/usr/bin/env python3
"""Snapshot the complete current AndroidX Compose Material 3 API surface.

The source of truth is the AndroidX repository.  This script deliberately
copies the canonical metalava/API signature files instead of maintaining a
hand-curated component list.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import urllib.request
from pathlib import Path

UPSTREAM_REPOSITORY = "androidx/androidx"
DEFAULT_REF = "cdeafe5b4e450d61cdb623d58c148efbb0d508cf"
MATERIAL3_ROOT = "compose/material3"
HERE = Path(__file__).resolve().parent
SNAPSHOT = HERE / "snapshot"


def request_bytes(url: str, token: str | None = None) -> bytes:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "utilities-android-phone-user-material3-api-snapshot",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request) as response:
        return response.read()


def api_json(path: str, token: str | None) -> object:
    return json.loads(
        request_bytes(f"https://api.github.com/repos/{UPSTREAM_REPOSITORY}/{path}", token)
    )


def child_tree_sha(tree_sha: str, name: str, token: str | None) -> str:
    tree = api_json(f"git/trees/{tree_sha}", token)
    for entry in tree["tree"]:
        if entry["path"] == name and entry["type"] == "tree":
            return entry["sha"]
    raise RuntimeError(f"tree {tree_sha} has no directory {name!r}")


def resolve_material3_tree(ref: str, token: str | None) -> tuple[str, str]:
    commit = api_json(f"commits/{ref}", token)
    resolved_ref = commit["sha"]
    root_tree = commit["commit"]["tree"]["sha"]
    compose_tree = child_tree_sha(root_tree, "compose", token)
    material3_tree = child_tree_sha(compose_tree, "material3", token)
    return resolved_ref, material3_tree


def classify(path: str) -> str:
    name = Path(path).name
    framed = f"/{path}/"
    if "/bcv/" in framed:
        return "binary-compatibility"
    if name == "current.txt":
        return "public"
    if name == "res-current.txt":
        return "resources"
    if name == "restricted_current.txt":
        return "restricted"
    if name == "public_plus_experimental_current.txt":
        return "public-plus-experimental"
    return "current-signature"


def is_current_api_signature(path: str) -> bool:
    framed = f"/{path}"
    in_api = "/api/" in framed and path.endswith("current.txt")
    in_bcv = "/bcv/" in framed and path.endswith("/current.txt")
    return in_api or in_bcv


def count_signature_items(text: str) -> dict[str, int]:
    return {
        "packages": len(re.findall(r"^package ", text, flags=re.MULTILINE)),
        "types": len(
            re.findall(
                r"^  (?:@[A-Za-z0-9_.$-]+(?:\([^\n]*\))? )*public .*\b(?:class|interface|enum|@interface)\b",
                text,
                flags=re.MULTILINE,
            )
        ),
        "methods": len(re.findall(r"^    method ", text, flags=re.MULTILINE)),
        "properties": len(re.findall(r"^    property ", text, flags=re.MULTILINE)),
        "fields": len(re.findall(r"^    field ", text, flags=re.MULTILINE)),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ref", default=DEFAULT_REF, help="AndroidX commit or ref to snapshot")
    args = parser.parse_args()

    token = os.environ.get("GITHUB_TOKEN")
    resolved_ref, material3_tree_sha = resolve_material3_tree(args.ref, token)
    recursive_tree = api_json(f"git/trees/{material3_tree_sha}?recursive=1", token)
    if recursive_tree.get("truncated"):
        raise RuntimeError("AndroidX Material 3 subtree listing was truncated")

    paths = sorted(
        entry["path"]
        for entry in recursive_tree["tree"]
        if entry["type"] == "blob" and is_current_api_signature(entry["path"])
    )
    if not paths:
        raise RuntimeError("No current Material 3 API signature files discovered")

    if SNAPSHOT.exists():
        shutil.rmtree(SNAPSHOT)
    SNAPSHOT.mkdir(parents=True)

    manifest_rows: list[tuple[str, str, int, str, str]] = []
    stats_rows: list[tuple[str, str, dict[str, int]]] = []

    for relative in paths:
        upstream_path = f"{MATERIAL3_ROOT}/{relative}"
        source = (
            "https://raw.githubusercontent.com/"
            f"{UPSTREAM_REPOSITORY}/{resolved_ref}/{upstream_path}"
        )
        data = request_bytes(source)
        destination = SNAPSHOT / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        kind = classify(relative)
        manifest_rows.append((kind, relative, len(data), digest, source))
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        stats_rows.append((kind, relative, count_signature_items(text)))

    license_source = (
        "https://raw.githubusercontent.com/"
        f"{UPSTREAM_REPOSITORY}/{resolved_ref}/LICENSE.txt"
    )
    (HERE / "UPSTREAM_LICENSE.txt").write_bytes(request_bytes(license_source))

    with (HERE / "MANIFEST.tsv").open("w", encoding="utf-8", newline="\n") as out:
        out.write("kind\tpath\tbytes\tsha256\tsource\n")
        for kind, path, size, digest, source in manifest_rows:
            out.write(f"{kind}\t{path}\t{size}\t{digest}\t{source}\n")

    source_record = {
        "upstream_repository": UPSTREAM_REPOSITORY,
        "requested_ref": args.ref,
        "resolved_commit": resolved_ref,
        "material3_root": MATERIAL3_ROOT,
        "signature_file_count": len(paths),
        "selection": [
            "compose/material3/**/api/*current.txt",
            "compose/material3/**/bcv/**/current.txt",
        ],
    }
    (HERE / "SOURCE.json").write_text(
        json.dumps(source_record, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    public_rows = [row for row in stats_rows if row[0] == "public"]
    totals = {
        key: sum(row[2][key] for row in public_rows)
        for key in ("packages", "types", "methods", "properties", "fields")
    }

    lines = [
        "# Material 3 current API index",
        "",
        f"Upstream: `androidx/androidx@{resolved_ref}`",
        "",
        f"Current signature files captured: **{len(paths)}**.",
        "",
        "The counts below are mechanical counts from the canonical public `api/current.txt` files. ",
        "They are an index only; the files under `snapshot/` are the authoritative API inventory.",
        "",
        "## Public API totals",
        "",
        "| Packages | Types | Methods | Properties | Fields |",
        "| ---: | ---: | ---: | ---: | ---: |",
        f"| {totals['packages']} | {totals['types']} | {totals['methods']} | {totals['properties']} | {totals['fields']} |",
        "",
        "## Signature files",
        "",
        "| Kind | Upstream-relative path | Packages | Types | Methods | Properties | Fields |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for kind, path, stats in stats_rows:
        lines.append(
            f"| {kind} | `{path}` | {stats['packages']} | {stats['types']} | "
            f"{stats['methods']} | {stats['properties']} | {stats['fields']} |"
        )
    lines.extend(
        [
            "",
            "## Reading the snapshot",
            "",
            "- `snapshot/material3/api/current.txt` is the core Compose Material 3 public API.",
            "- Other `api/current.txt` files are public APIs of Material 3 companion modules.",
            "- `res-current.txt` files record Android resource API.",
            "- `restricted_current.txt` files are AndroidX restricted surfaces and are not ordinary app-facing API.",
            "- `public_plus_experimental_current.txt`, where present upstream, is retained rather than silently discarded.",
            "- `bcv/**/current.txt` files are binary-compatibility signatures retained for completeness.",
            "",
        ]
    )
    (HERE / "INDEX.md").write_text("\n".join(lines), encoding="utf-8")

    print(f"snapshotted {len(paths)} signature files from {resolved_ref}")


if __name__ == "__main__":
    main()
