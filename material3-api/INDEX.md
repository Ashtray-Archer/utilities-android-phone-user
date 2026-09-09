# Material 3 current API index

Upstream: `androidx/androidx@cdeafe5b4e450d61cdb623d58c148efbb0d508cf`

Current signature files captured: **43**.

The counts below are mechanical counts from the canonical public `api/current.txt` files. 
They are an index only; the files under `snapshot/` are the authoritative API inventory.

## Public API totals

| Packages | Types | Methods | Properties | Fields |
| ---: | ---: | ---: | ---: | ---: |
| 13 | 456 | 3403 | 1382 | 145 |

## Signature files

| Kind | Upstream-relative path | Packages | Types | Methods | Properties | Fields |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| public | `adaptive/adaptive-layout/api/current.txt` | 1 | 72 | 209 | 101 | 21 |
| resources | `adaptive/adaptive-layout/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `adaptive/adaptive-layout/api/restricted_current.txt` | 1 | 72 | 209 | 101 | 21 |
| binary-compatibility | `adaptive/adaptive-layout/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `adaptive/adaptive-navigation/api/current.txt` | 1 | 6 | 42 | 9 | 1 |
| resources | `adaptive/adaptive-navigation/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `adaptive/adaptive-navigation/api/restricted_current.txt` | 1 | 6 | 42 | 9 | 1 |
| binary-compatibility | `adaptive/adaptive-navigation/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `adaptive/adaptive-navigation3/api/current.txt` | 1 | 10 | 51 | 16 | 2 |
| resources | `adaptive/adaptive-navigation3/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `adaptive/adaptive-navigation3/api/restricted_current.txt` | 1 | 10 | 51 | 16 | 2 |
| binary-compatibility | `adaptive/adaptive-navigation3/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `adaptive/adaptive/api/current.txt` | 1 | 7 | 28 | 15 | 0 |
| resources | `adaptive/adaptive/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `adaptive/adaptive/api/restricted_current.txt` | 1 | 7 | 28 | 15 | 0 |
| binary-compatibility | `adaptive/adaptive/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `material3-a2ui/api/current.txt` | 2 | 7 | 32 | 15 | 2 |
| resources | `material3-a2ui/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `material3-a2ui/api/restricted_current.txt` | 2 | 7 | 32 | 15 | 2 |
| public | `material3-adaptive-navigation-suite/api/current.txt` | 1 | 11 | 70 | 27 | 3 |
| public-plus-experimental | `material3-adaptive-navigation-suite/api/public_plus_experimental_current.txt` | 0 | 0 | 0 | 0 | 0 |
| resources | `material3-adaptive-navigation-suite/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `material3-adaptive-navigation-suite/api/restricted_current.txt` | 1 | 11 | 70 | 27 | 3 |
| public | `material3-ripple/api/current.txt` | 1 | 15 | 21 | 20 | 4 |
| resources | `material3-ripple/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `material3-ripple/api/restricted_current.txt` | 1 | 15 | 21 | 20 | 4 |
| binary-compatibility | `material3-ripple/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `material3-window-size-class/api/current.txt` | 1 | 8 | 27 | 12 | 3 |
| resources | `material3-window-size-class/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `material3-window-size-class/api/restricted_current.txt` | 1 | 8 | 27 | 12 | 3 |
| binary-compatibility | `material3-window-size-class/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `material3/api/current.txt` | 3 | 318 | 2921 | 1167 | 109 |
| resources | `material3/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `material3/api/restricted_current.txt` | 3 | 318 | 2921 | 1167 | 109 |
| binary-compatibility | `material3/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `xr/xr-adaptive/api/current.txt` | 1 | 2 | 2 | 0 | 0 |
| resources | `xr/xr-adaptive/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `xr/xr-adaptive/api/restricted_current.txt` | 1 | 2 | 2 | 0 | 0 |
| binary-compatibility | `xr/xr-adaptive/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |
| public | `xr/xr/api/current.txt` | 0 | 0 | 0 | 0 | 0 |
| resources | `xr/xr/api/res-current.txt` | 0 | 0 | 0 | 0 | 0 |
| restricted | `xr/xr/api/restricted_current.txt` | 0 | 0 | 0 | 0 | 0 |
| binary-compatibility | `xr/xr/bcv/native/current.txt` | 0 | 0 | 0 | 0 | 0 |

## Reading the snapshot

- `snapshot/material3/api/current.txt` is the core Compose Material 3 public API.
- Other `api/current.txt` files are public APIs of Material 3 companion modules.
- `res-current.txt` files record Android resource API.
- `restricted_current.txt` files are AndroidX restricted surfaces and are not ordinary app-facing API.
- `public_plus_experimental_current.txt`, where present upstream, is retained rather than silently discarded.
- `bcv/**/current.txt` files are binary-compatibility signatures retained for completeness.
