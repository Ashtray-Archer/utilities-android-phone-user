# Civics

A small public-record feed for things local and state government is actually doing.

The first target is not a recommendation system and not a replacement for the
underlying records. It collects official public material, keeps the fetched
response, derives searchable text, and lets a person count words or phrases.
Later statistical or machine-learning annotations should remain a derived layer
with links back to the exact source material that produced them.

## First jurisdictions

- Michigan Legislature: bills and committee meetings
- Washtenaw County, Michigan: Board of Commissioners agendas and minutes
- Ann Arbor, Michigan: City Council and other public-body records
- Monroe County, Indiana: Board of Commissioners and County Council
- Bloomington, Indiana: City Council meeting files
- Detroit, Michigan: City Council agendas and documents
- Chicago, Illinois: City Clerk eLMS meetings and legislative matters

The initial source catalogue is in [`sources.tsv`](sources.tsv). Prefer an
official government source as the primary record. Mirrors may be useful later
for audit, cross-checking, or historical coverage, but should not silently
replace the primary source.

## Filesystem model

Fetched material is append-first:

```text
data/
  <source_id>/
    20260927T160000Z.body
    20260927T160000Z.text.txt
    20260927T160000Z.links.txt
    20260927T160000Z.meta.json
```

`.body` is the response body as received. It is the local source of truth.
`.text.txt` and `.links.txt` are replaceable derived views.
`.meta.json` records source ID, URL, retrieval time, byte count, and SHA-256.

This is deliberately filesystem-first. No database is required for the first
version.

## Draft scraper

[`scrape.pi`](scrape.pi) is Ithon source. Its intended commands are:

```text
ithon scrape.pi list
ithon scrape.pi fetch ann_arbor_legislative
ithon scrape.pi fetch-all
ithon scrape.pi words ann_arbor_legislative housing zoning police
ithon scrape.pi words chicago_elms_matters "lead service line"
```

`words` operates on the newest derived text snapshot and prints counts. Search
terms come from the person using the program; the scraper does not ship a
political importance score.

The generic scraper also extracts likely agenda/minute/packet/PDF links from
HTML pages. That gives us a first crawl frontier without pretending that every
government site has the same structure. Source-specific adapters should replace
generic link discovery where a stable structured endpoint exists.

## Evidence status

- Branch and source catalogue: implemented.
- Generic raw snapshot/text/link/word-count code: drafted on this branch.
- Source-specific structured adapters: not yet implemented.
- Live end-to-end fetches on the phone: not yet accepted.
- Android APK/feed UI: not yet implemented.

Do not relabel a source as working merely because its public page exists. A
source becomes accepted only after the exact fetch and parse path runs and
produces a receipt tied to that revision.
