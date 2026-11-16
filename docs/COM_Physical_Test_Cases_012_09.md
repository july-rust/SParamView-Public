# 012_09 Physical S4P → TX/PCB/RX Complete Two-Case Circuit Pipeline

**Status: analytic, electrically valid circuit-model implementation, not 802.3ck-2022 COM compliance.**

## Actual performed computation

`calculateComPhysicalTestCases()` consumes an explicitly confirmed 4-port
complex S-parameter matrix at each measured frequency, ordered as
`nearP, nearN, farP, farN`. It requires a measured DC sample, identical
positive real 50-ohm nominal physical port references (or another explicitly
common positive real value), passive 4-port network and an explicit bound
on allowed mode conversion. Missing band/grid/reference data or active
models are rejected, not interpolated or silently fixed.

Each frequency uses unitary physical 4-port-to-differential S conversion;
then the actual complex Redheffer star product of the explicitly defined
TX balanced package, differential board network and RX balanced package.
Both the **forward and reverse** transmission and **both end reflections**
are retained. It computes **both Test 1 and Test 2 networks**, independently,
using their distinct nominal primary line lengths:

| Package case | TX primary line (mm) | RX primary line (mm) |
|---|---:|---:|
| Test 1 | 13 | 11 |
| Test 2 | 31 | 29 |

All **other** Cd, Cb, Cp, Ls, line 2 length, Zc/Zc2, delay and alpha(f)
parameters must be explicitly supplied. The caller sets primaryLineMm=0
to acknowledge the test selector override, otherwise it fails closed.
The circuit is a physically valid linear analog network implementation
but is NOT an auditable final IEEE 93A package gamma/port termination
simulation or COM margin.

## Direct verified Touchstone S4P path

`physicalComInputFromSource(SourceSnapshot, victimChannelId, ...)`
requires an actual four-port differential mapping with all four physical
port indices unique. It uses the SHA-verified `SourceSnapshot` and reads
the original Touchstone `Cache::trace` across **all 16 measured Sij**
frequency responses. The original per-physical-port real reference
impedance is retained; it does not reconstruct Sdd11 from Sdd21 or
guess port ordering. A bounded extraction and cancelable worker are enforced.

`calculateComPhysicalFromSource(...)` connects the complete actual
source to both physical package cases, propagating the original SHA-256,
revision and mapping/workspace generations. In GUI operation, the caller
must still compare SourceSnapshot identity against the live workspace
before publishing results or persisting cache entries.

An integration regression test generates a synthetic **.s4p Touchstone
file on disk**, parses it into the real cache, captures and SHA-verifies the
confirmed mapping, and compares both full package calculations against
an independently derived 0.8 complex THRU. It also checks source
provenance, invalid mapping and cancellation.

## Independent analytical oracles

Matched balanced lines: for physical board transmission B(f) and two
matched package models with identical gamma=alpha+j*2*pi*f*tau,

`Sdd21_TestK(f) = B(f) * exp(-gamma*(Ltx_K+Lrx_K+2*L2))`

and `Sdd12` is equal, `Sdd11=Sdd22=0`. The executable test checks
both package cases across DC→16 GHz against this independent equation.

For a passive mismatched board, Sdd11/Sdd22 are preserved when package
elements and propagation are set to ideal; non-passive boards, unequal
frequency-dependent reference impedances, omitted DC, excess mode conversion,
partial 4-port frequency grids, invalid package inputs and cancellation
must fail closed. Full-platform CI tests reproduce all assertions.

## Not a numerical COM dB result

No IEEE 93A transmitter equalizer/receiver CTLE/DFE search, PAM4 pulse
and timing PDF, spectral jitter and cross-talk budget, FOM, error-cdf
or COM dB is computed here. `calculateCom()` still returns
`Status::NotEvaluated` with no numeric result. This entire pipeline is
exposed **only** as a warning-marked diagnostic and cannot be enabled as
a certified IEEE 802.3ck COM result without final-standard qualification.

The normative IEEE Std 802.3ck-2022 Table 120F-8 full parameter audit
and three separate independent same-PHY numerical Golden channels for
both package cases are still blockers; the historical August 2022 workbook
is reference provenance only.

## Sources

- [IEEE 802.3ck-2022 active standard](https://ieeexplore.ieee.org/document/9999414)
- [IEEE 2022 public task-force archive](https://www.ieee802.org/3/df/public/tools/index.html)
- [COM historical workbook evidence](COM_2022_SA_Reference_Audit_012_09.md)
- [COM numerical gate and result status](Channel_Analysis_COM_012_09.md)
