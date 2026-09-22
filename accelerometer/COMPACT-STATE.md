# Compact accelerometer state

## Physical object

Android supplies one binary32 measurement `a = (x, y, z)`. The codec first subtracts the balanced gravity point

`c = −(10/√3)(1, 1, 1)`

to obtain the residual `r = a − c`. A nonzero residual is split into its magnitude `ρ = ‖r‖` and its unit direction `u = r/ρ`. The direction is the unit pure quaternion `uₓi + uᵧj + u_zk`; it has two independent degrees of freedom and is not expanded into a redundant four-component quaternion.

The reference is evaluated from `10` and `√3`. The human approximation `40/7` is not part of the codec.

## Retained bytes

The retained record is four bytes:

| Bytes | Meaning |
| --- | --- |
| 3 | Two signed Q0.11 coordinates in the standard octahedral chart of `S²`, packed as adjacent 12-bit two's-complement integers |
| 1 | Unsigned residual-magnitude code `m` |

The magnitude meaning is exactly

`ρ = m/(4√3) m/s²`.

The direction chart uses a dyadic lattice. It does not divide byte values by `127` or `255`, and it does not contain a hidden `/256` physical scale. Decoding unfolds the octahedron and normalizes the resulting three coordinates onto `S²`.

Magnitude zero has one canonical encoding: all four bytes zero. A zero magnitude with nonzero direction bytes is rejected as noncanonical, and the decoder leaves its output untouched. Every direction bit pattern is otherwise defined.

Finite residuals beyond `255/(4√3) m/s²` return an explicit saturation result and produce magnitude code `255`; they never wrap. Nonfinite Android input is rejected and leaves the previously retained state untouched.

## Why the intended three-byte record was rejected

The intended record used two signed Q0.7 octahedral direction coordinates and the same magnitude byte. That is the natural three-byte dyadic design, but it does not support the requested visible precision across the magnitude byte's domain.

The deterministic study covers 8,660,069 samples: a Fibonacci sweep across `S²` at every magnitude code, the radius-10 gravity sphere, integer hand-scale translations around seven gravity orientations, and both sides of the octahedral fold boundary. Its results are:

| Representation | Maximum component error | RMS component error | Maximum residual-magnitude error |
| --- | ---: | ---: | ---: |
| Rejected Q0.7 direction + magnitude byte | `0.538368 m/s²` | `0.088690 m/s²` | `0.072168 m/s²` |
| Production Q0.11 direction + magnitude byte | `0.077181 m/s²` | `0.040913 m/s²` | `0.072168 m/s²` |

The screen quantum is `1/7 m/s² = 0.142857… m/s²`. The three-byte candidate's worst component error is several screen steps. Seventh rounding would conceal rather than repair that loss. Q0.11 keeps the measured maximum component error below 75 percent of one screen quantum over the tested domain. Since two Q0.11 coordinates require 24 bits, four bytes are the smallest byte-aligned form of this direct dyadic octahedral correction.

These are deterministic host results, not physical-phone accuracy evidence. A MIRO A1 run still has to compare the transient Android accelerometer reading with the decoded compact state.

## Live path and presentation

The live path is:

`Android binary32 measurement → compact encode → four retained bytes → compact decode → reconstructed x/y/z → nearest-seventh presentation`.

The application state contains the compact record, not a retained copy of the Android binary32 axes or `_Float16` shadow axes. Periodic logs print the current Android accelerometer reading beside the decoded value solely as an oracle for later physical acceptance.

The renderer rounds each decoded component by `round(7a)` and presents the sign, integral part, and numerator `0` through `6`. Integral values omit `0/7`; signed values near zero are rounded before their sign is chosen, so a value that rounds to zero is displayed as `+0` rather than `−0`.

## Host evidence

`tests/compact_acceleration_test.c` checks the byte size, balanced and antipodal points, axis directions, unit-sphere reconstruction, all 256 magnitude meanings, zero normalization, saturation, nonfinite and malformed input, the retained-state-only screen model, and seventh formatting.

`tests/error_study.c` performs the deterministic sweep summarized above and fails unless the rejected Q0.7 candidate exceeds one screen quantum while the production codec retains a 25 percent margin below it.

`tests/accelerometer_snapshot_test.c` checks the shared semantic path used by command-line inspection: Android accelerometer reading → difference from balanced gravity → compact state and unit pure residual-direction quaternion → reconstruction → screen sevenths.
