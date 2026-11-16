# 012_09 — Causal dispersive reference package model and physical impedance correction

**Status: implemented and tested physical equation only; IEEE 802.3ck COM dB remains NOT_EVALUATED.**

## Why the previous physical package response was insufficient

The older `calculateBalancedComPackage()` accepts `attenuationSqrtGHzPerMm`/`attenuationGHzPerMm` and a constant group delay. Its line model sets imaginary gamma to only 2πfτ; the frequency-dependent imaginary dispersion from the a1 and a2 terms is absent. Its API also treats the primary Zc as **single-ended** and doubles it in the differential equivalent. The public IEEE COM package reference figures define the line Zc as already differential, because the reference reflection coefficient is `(Zc-2*R0)/(Zc+2*R0)`.

That old function and its non-normative tests are retained unchanged for existing clients. Its output MUST NOT be called a 802.3ck package simulation.

## New physically explicit C++20 API

- `comCausalGammaPerMm()`: exact published causal-by-design complex propagation constant
- `comCausalLine()`: stable direct reciprocal S11, S21, S12, S22 with internal reflections for unequal Zc/Zref
- `calculateComCausalPackage()`: Cd/Ls/Cb, two dispersive Zc lines, Cp; reverse order for RX
- `historical8023ckC2cCausalCandidate()`: typed public-comparison parameter candidate, **not** certified final Table 120F-8

Normalized frequency `x=f/(1 GHz)`, propagation in 1/mm:

```text
gamma(0) = gamma0
gamma(f>0) = gamma0 + a1*(1+j)*sqrt(x)
                 + [a2*(1-j*(2/pi)*ln(x)) + j*2*pi*tau_ns]*x
rho = (Zc_diff - Zref_diff)/(Zc_diff + Zref_diff)
u = exp(-gamma*length_mm)
Sdd21 = Sdd12 = (1-rho*rho)*u / (1-rho*rho*u*u)
Sdd11 = Sdd22 = rho*(1-u*u) / (1-rho*rho*u*u)
```

Unit dimensions: `a1` sqrt(ns)/mm, `a2` ns/mm, `tau` ns/mm, `gamma0` /mm; `Zc` and `Zref` **differential Ω**. The die shunt C and series L remain **single-ended per conductor**, then enter the differential balanced network as C/2 and 2L. `R_d` stays an external COM termination and is not silently synthesized in the package model.

## Cross-referenced historical C2C candidate (NOT APPROVED FINAL LOCK)

| Parameter | Historical comparison value | SI C++ value |
|---|---:|---:|
| Cd (single-ended) | 0.00012 nF | 1.2e-13 F |
| Ls (single-ended) | 0.12 nH | 1.2e-10 H |
| Cb (single-ended) | 0.00003 nF | 3.0e-14 F |
| Cp (single-ended) | 0.000087 nF | 8.7e-14 F |
| Zc1, Zc2 (differential) | 87.5, 92.5 Ω | 87.5, 92.5 Ω |
| Secondary line | 1.8 mm | 1.8 mm |
| γ0 | 0 /mm | 0 /mm |
| a1, a2 | 9.909e-4 sqrt(ns)/mm; 2.772e-4 ns/mm | same |
| τ | 0.006141 ns/mm | 0.006141 ns/mm |
| Test 1 primary line Tx/Rx | 13/11 mm | external Test1 selector |
| Test 2 primary line Tx/Rx | 31/29 mm | external Test2 selector |

These public numbers agree across an Ansys public C2C COM 3.40 settings page, an IEEE task-force historical parameter comparison and a Samtec tutorial. They are not an independent byte-verified full IEEE Std 802.3ck-2022 Table120F-8. In particular, no COM output is represented as compliance.

## End-to-end S4P and actual SourceSnapshot integration

The new `com_causal_channel.hpp/cpp` API connects this corrected
causal package model to the **full 4×4 complex board S-parameter matrix**:
`calculateHistoricalCausalC2cCases()` handles a directly provided
measured S4P frequency grid, while
`calculateHistoricalCausalC2cFromSource()` reads **all 16 physical Sij**
from a SHA-verified `SourceSnapshot` using confirmed differential
4-port mapping. The source revision, SHA, workspace and mapping generation
are retained, and the caller must still check that the live workspace
identity has not changed before displaying results.

Both Test1 (Tx/Rx 13/11 mm) and Test2 (31/29 mm) use the causal
historical C2C package parameters, differential 87.5/92.5 Ω lines,
and a full Redheffer TX–board–RX multi-reflection cascade. The result
returns four complex Sdd parameters per frequency for both cases,
`Status::Warning`, and `NOT_IEEE_COM`, and enforces a 50 Ω physical
port reference for this profile. It deliberately does NOT produce
an IEEE-compliant COM dB estimate.

Tests `channel_com_causal_s4p_pipeline` include frequency-domain
physical network assembly checks, 50 Ω reference/mode/passivity failure,
cancellation, and a real synthetic on-disk Touchstone S4P → Cache →
pinned SourceSnapshot → both loaded C2C package cases round-trip.

## Regression evidence required

CTest `channel_com_causal_package` checks each of seven DC-to-53.125 GHz frequencies against the independently derived real/imaginary gamma formula, matched e^(-gamma*l) complex transfer and unmatched ABCD-scattering oracle; protects the a1 phase and a2 log dispersion from regressions; checks differential 87.5-Ω (not 175-Ω) and both package orientations; high-loss stability, nonfinite values, invalid direction, and loss/noise fail-closed.

CI: Linux Release and ASan/UBSan, Windows x64, Windows ARM64/native package, macOS arm64, and code review before squash merge. No release, no `calculateCom()` switch.

## Primary public sources

- [Samtec IEEE COM training — causal gamma and Zc/2R0 matching](https://blog.samtec.com/wp-content/uploads/2020/07/07_02_2020_geek_speek_IEEE_COM.pdf)
- [Ansys published 802.3ck C2C COM 3.40 settings](https://ansyshelp.ansys.com/public/Views/Secured/Electronics/v251/en/Subsystems/Circuit/Content/Circuit/COMAnalysisSpec8023CK_100GAUI_1_C2C_200GAUI_2_400GAUI_4_C2C.htm)
- [IEEE task-force 112G package parameters comparison](https://www.ieee802.org/3/dj/public/23_0720/lim_3dj_02a_2307.pdf)
- [Original 012_09 COM qualification state](Channel_Analysis_COM_012_09.md)
