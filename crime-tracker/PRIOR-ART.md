# Crime-tracker prior art

This branch is primarily for learning before deciding whether there should be a
phone utility at all.

## Public-data side

### CityOfDetroit/crime-viewer

<https://github.com/CityOfDetroit/crime-viewer>

Official City of Detroit project for viewing, filtering, and summarizing police
incident data. It is useful evidence that a municipal crime feed can be treated
as a visualization of published incident records rather than as a scanner.

The repository currently has no `LICENSE` file at its root. Treat it as
architecture/UI reference only unless licensing is clarified; do not copy its
source into this repository.

Detroit also maintains a public open-data portal with public-safety datasets:
<https://data.detroitmi.gov/>.

### openpolicedata/openpolicedata

<https://github.com/openpolicedata/openpolicedata>

Useful reference for the *source adapter* problem: many agencies publish
incident-level data through different ArcGIS/Socrata/filesystem endpoints, and
the project preserves links back to original agency data.

License: BSD 3-Clause.

Do not import its Python package into this phone project. If we need the same
idea, inspect the source catalogue and adapters and re-express only the useful
parts in this project's language/runtime model.

## Radio/scanner side

### TrunkRecorder/trunk-recorder

<https://github.com/TrunkRecorder/trunk-recorder>

Useful acquisition reference. It separates RF capture/decoding from the files
produced for each radio call. It supports P25, SmartNet, DMR, and analog sources
through SDR hardware and GNU Radio/OP25.

License: GPL-3.0.

This is much heavier than a phone-side viewer, but its recorder/output boundary
is worth preserving if scanner work ever proceeds.

### chuot/rdio-scanner

<https://github.com/chuot/rdio-scanner>

Useful presentation/distribution reference. It consumes files produced by radio
recorders such as Trunk Recorder or SDRTrunk instead of owning RF acquisition
itself. That separation is attractive: a future phone utility could consume a
feed without embedding an SDR stack.

License: GPL-3.0.

## Initial direction

Keep two acquisition families separate:

```text
published incident data -> provenance-preserving incident records -> map/list
radio receiver/recorder -> per-call audio + metadata -> scanner-style viewer
```

Do not collapse a dispatch call, incident report, arrest, charge, or conviction
into the same event type. They are distinct records with different meanings.

The first implementation candidate should be published incident/open data.
Scanner/radio ingestion can remain a later experiment.
