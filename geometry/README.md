# Compact unit direction

This directory owns the reusable part of the accelerometer compact-state
representation.

Every nonzero vector in `ℝ³` determines a direction on `S²`. A unit direction
can equivalently be read as the unit pure quaternion

`0 + x i + y j + z k`.

`compact_unit_direction.h` stores that direction in exactly three bytes: two
signed Q0.11 octahedral square coordinates packed as adjacent 12-bit
two's-complement integers. The square is a folded octahedral parameterization
with seams, not one ordinary global manifold chart on `S²`. Every 24-bit stored
pattern decodes to a direction on `S²`.

The representation deliberately contains **no magnitude, physical unit,
balanced-gravity reference, sensor identity, or full phone orientation**. A
general unit quaternion lies on `S³`; it represents an element of `SO(3)` only
after the identification `q ~ −q`. That is a different storage contract.

The accelerometer retains one of these three-byte directions plus its own
one-byte residual magnitude. Gyroscope and magnetometer work may reuse this
direction codec when they need compact direction state without inheriting
accelerometer semantics.

## Executable mathematical decomposition

The encoder is written as the composition

`ℝ³ \ {0} → {x ∈ ℝ³ : ‖x‖₁ = 1} → octahedral square → Q0.11² → 24 bits`.

Its named C stages project a nonzero vector onto the unit `L¹` octahedron,
fold the lower octahedral hemisphere into the square, quantize each coordinate,
and pack the two signed 12-bit codes. The projection first divides by `‖v‖∞`
to avoid overflow while calculating `v/‖v‖₁`:

`(v/‖v‖∞) / ‖v/‖v‖∞‖₁ = v/‖v‖₁`.

That first division is numerical protection, not a second change of direction.

Decode names the corresponding reverse stages:

`24 bits → Q0.11² → octahedral square → {x ∈ ℝ³ : ‖x‖₁ = 1} → S²`.

Packing and unpacking are exact inverses for the bounded signed 12-bit codes.
Fold and unfold are the paired geometric stages before rounding. Q0.11
quantization loses information, and Float32 arithmetic adds rounding, so a full
encode followed by decode is only an approximation to the original unit
direction.

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
