# Compact unit direction

This directory owns the reusable part of the accelerometer compact-state
representation.

A nonzero three-dimensional direction lies on `S²` and can equivalently be
read as the unit pure quaternion

`0 + x i + y j + z k`.

`compact_unit_direction.h` stores that direction in exactly three bytes:
two signed Q0.11 coordinates in the standard octahedral chart of `S²`, packed
as adjacent 12-bit two's-complement integers. Every 24-bit stored pattern
decodes to a direction on `S²`.

The representation deliberately contains **no magnitude, physical unit,
balanced-gravity reference, sensor identity, or full phone orientation**. A
general unit orientation quaternion lies on `S³` and needs a different
storage contract.

The accelerometer retains one of these three-byte directions plus its own
one-byte residual magnitude. Gyroscope and magnetometer work may reuse this
direction codec when they need compact direction state without inheriting
accelerometer semantics.

## Fixed byte fixtures

The current codec fixes these axis encodings:

| direction | bytes low middle high |
| --- | --- |
| +X | `ff 07 00` |
| -X | `00 08 00` |
| +Y | `00 f0 7f` |
| -Y | `00 00 80` |
| +Z | `00 00 00` |
| -Z | `ff f7 7f` |

The host test also sweeps 131,072 directions on `S²`, requires decoded
directions to remain unit length, and retains the existing Q0.11 maximum
component-error bound.

The source algorithm was extracted mechanically from the settled accelerometer
codec on `main`; the accelerometer-specific four-byte record remains the
acceptance oracle for its physical magnitude and reference semantics.
