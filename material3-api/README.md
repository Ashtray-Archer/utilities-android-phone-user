# AndroidX Compose Material 3 API

This directory records the complete **current** AndroidX Compose Material 3 API surface as canonical upstream signature files.

It exists as a reference for phone-side UI work, including IB pre-paint experiments.  It is deliberately not a hand-written list of familiar widgets: hand-maintained lists omit overloads, defaults, state objects, colors, shapes, adaptive APIs, experimental surfaces, resource APIs, and newer companion modules.

## Source of truth

The snapshot is generated from the AndroidX repository:

- repository: `androidx/androidx`
- Material 3 source root: `compose/material3`
- pinned snapshot ref: `cdeafe5b4e450d61cdb623d58c148efbb0d508cf`
- upstream branch at the time of capture: `androidx-main`

`update_material3_api.py` discovers the Material 3 subtree from the pinned commit and copies every current signature file matching:

- `compose/material3/**/api/*current.txt`
- `compose/material3/**/bcv/**/current.txt`

This includes the ordinary public API (`api/current.txt`) and, for completeness, upstream resource, restricted, experimental, and binary-compatibility current signatures where those files exist.

The generated files are placed under `snapshot/` while preserving their paths relative to `compose/material3/`.

## What counts as the API

The authoritative inventory is the upstream signature corpus, not this README.

The most immediately relevant file for an ordinary Android app is:

`material3-api/snapshot/material3/api/current.txt`

That file contains the complete current public API of the core `androidx.compose.material3` artifact: composable functions, classes, interfaces, state types, defaults objects, colors, shapes, typography, motion, layout helpers, menus, dialogs, buttons, controls, app bars, navigation, sheets, pickers, tooltips, text fields, and the rest of the package surface, including overloads and annotations.

Other `api/current.txt` files under the snapshot are public APIs of Material 3 companion modules such as adaptive/layout/navigation modules, window-size support, XR, A2UI, and whatever additional Material 3 modules exist at the pinned AndroidX commit.  The updater discovers those modules rather than relying on a remembered list.

Files named `restricted_current.txt` are AndroidX restricted surfaces; their presence here does **not** mean IB or another application should depend on them.  `res-current.txt` files record Android resource API.  `bcv/**/current.txt` files record native/binary compatibility surfaces.

## Generated index and integrity data

Running the updater also writes:

- `INDEX.md` — mechanical counts and a per-signature-file index;
- `MANIFEST.tsv` — kind, path, byte count, SHA-256, and exact pinned source URL for every copied signature;
- `SOURCE.json` — the resolved AndroidX commit and discovery rule;
- `UPSTREAM_LICENSE.txt` — the AndroidX Apache 2.0 license copied from the same pinned commit.

The copied signature files are intentionally left verbatim.  Do not normalize, prettify, translate, or collapse overloads in the snapshot itself.

## Updating

To refresh deliberately against a newer AndroidX state:

```sh
python3 material3-api/update_material3_api.py --ref androidx-main
```

For a reproducible update, resolve `androidx-main` to a commit first and then update `DEFAULT_REF` and this README to that exact SHA before committing the resulting snapshot.

The branch workflow runs the pinned updater and commits the generated corpus when the updater/workflow is changed.

## Relation to IB

This is a platform API reference, not an assertion that IB should expose Material 3 one-for-one.

For IB, the likely architecture remains:

```text
IB semantic object
        |
        +-- document/pre-paint renderer
        |
        +-- small platform-control vocabulary
                 |
                 +-- Android Material 3 adapter
```

The point of keeping the full API here is to make the adapter design evidence-based.  We can identify the smallest semantic subset IB actually needs without pretending that `Button`, `Card`, `TextField`, and `Scaffold` are the whole Material 3 system.
