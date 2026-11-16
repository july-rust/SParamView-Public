# 012_09 — Historical IEEE C2C real-channel numerical reference qualification

**Current status (2026-11-17 KST): HISTORICAL NUMERICAL REFERENCE CANDIDATE — COMPLETE; normative IEEE and native SParamView C++ COM — NOT COMPLETE.**

The original 2019 IEEE 802.3ck public channel data and August 2022 historical C2C workbook have now been independently evaluated by both the pinned SiCoPR COM 4p17p0 Python port and a separate GNU Octave-compatible 4p17p0 reference implementation. Six real candidate package-case pairs agree numerically. These are **not** three qualified published-final 802.3ck goldens, and **not** native C++ results.

## Actual independently calculated results

Executed [full CI #43972559021](https://github.com/july-rust/SParamView-Public/actions/runs/43972559021): **three Python physical-channel jobs + three GNU Octave jobs all SUCCESS**, including metadata SHA, physical port mapping, no-complex-FOM and Test1/Test2 comparison gates.

| IEEE source loss label | Test1 candidate COM (dB) | Test2 candidate COM (dB) | Largest same-input Python/Octave COM difference (dB) |
|---|---:|---:|---:|
| 10 dB | 4.842540704356086 | 4.697201007609771 | 5.60e-14 |
| 16 dB | 4.293845073865506 | 3.9994128151173114 | 1.17e-13 |
| 18 dB | 4.054649183385672 | 3.582840211206002 | 2.66e-15 |

Independent **FOM** values also agree (largest observed absolute difference under 3e-13 dB). The programmatic tolerance for both candidate COM and FOM is 1e-4 dB; it is used only for this historical reproducibility test, not as a general IEEE tolerance. All six Octave outputs were explicitly checked for zero imaginary components, with no silent complex-to-real casting.

Exact executable paths: `scripts/com_3ck_c2c_real_reference.py`, `scripts/com_3ck_c2c_octave_check.py`, `scripts/com_3ck_c2c_compare_numerics.py`. The test suite includes negative cases for source identity, incomplete package pairs, mismatched port map, unsupported/duplicated settings, non-real FOM, incorrect independent values and formula injection.

## Root cause of previous complex-FOM failure: physical source port mapping

The historical source S4P files have physical differential TX ports **1/2** and RX ports **3/4**, while the reference tool's generic/default `Port Order=[1 3 2 4]` interprets differential TX as 1/3 and RX as 2/4. On the original channel files the default produced very weak LF differential transmission and negative sampled cursor amplitudes, then complex log FOM values which the SiCoPR optimizer could not convert to float.

The original source data was measured under all three alternative four-port differential pair partitions and a fail-closed SHA/physics-based port mapping contract was added:

| Channel | 10 MHz `|Sdd21|` with default [1,3,2,4] | 10 MHz `|Sdd21|` with [1,2,3,4] | 26.56 GHz `|Sdd21|` with [1,2,3,4] |
|---|---:|---:|---:|
| 10 dB | 0.00546 | 0.98617 | 0.30917 |
| 16 dB | 0.00860 | 0.97695 | 0.16225 |
| 18 dB | 0.00986 | 0.97314 | 0.12332 |

The source SHA, low-frequency transmission, and 26.5625 GHz channel-loss label are all checked before an **explicit temporary CSV `Port Order=[1 2 3 4]`** is allowed. This mapping override is recorded and provided identically to Python and Octave. It does **not** rewrite any official source XLSX or S4P file, does **not** change source numerical parameters beyond the explicit port map, and is **not** assumed to be a published final-table normative profile.

An earlier 1-FEXT/1-NEXT input mix also differed from the historical [IEEE 2020 P802.3ck 4-FEXT / 0-NEXT contribution, slide 9](https://www.ieee802.org/3/ck/public/20_03/healey_3ck_01a_0320.pdf). The published 2019 source ZIP contains one THRU, one FEXT and one NEXT per loss group. The tested historical stimulus therefore uses **four instances of the single SHA-pinned FEXT** and zero NEXT. This does not claim four independent distinct aggressor files.

## Exact pinned source provenance and redistribution

- [IEEE 2019 public C2C archive](https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip), archive SHA-256 `670fe10711b5c530320f770d22a717387b161f2721a2203b27e84ebf7ac597e2`; each actual THRU/FEXT/NEXT member is individually SHA-verified. The 20 dB channel is SHA-pinned but not part of the required 3-case automatic run.
- [IEEE 2022 historical COM archive](https://www.ieee802.org/3/df/public/tools/COM/mellitz_3df_02_2211.zip), ZIP SHA-256 `895857fbfb602c8ec10b221ab40a23d4b39244b4b57101dd27abc4b2a1a2fddc`; exact Strict OOXML C2C workbook SHA-256 `e676b3fb3cb3048f80c98deaa8faca1d03c13daa216c6259de26885e715ca925`.
- Source workbook's sole explicit `COM_Settings` sheet is determined through its Strict OOXML relationship. All **eight** spreadsheet arithmetic formulas are recomputed with a bounded non-`eval` parser and checked against the workbook's cached numeric values; original temporary canonical CSV SHA-256 `2926eedd529853fd36a1d76d25d953981df062703ade0394ef95607905f18e98`.
- [SiCoPR BSD-3 source commit](https://github.com/tbermen/SiCoPR/tree/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75) is pinned in both Python and GNU Octave CI. Octave path uses the original COM 4p17p0-derived compatibility implementation, which is a **historical independent numerical crosscheck**, not IEEE normative endorsement.
- Official IEEE ZIP/S4P/XLSX and external COM source **never enter this repo or release assets**. Workflows use disposable runner storage; artifacts contain numerical results, verified SHA/port metadata and source-free comparisons only.

## Acceptance distinction and remaining blocking work

**This PR's historical numerical reference candidate acceptance:** real SHA-pinned source, 3 channel families, both package cases, exactly matched source/port/filter settings, 6 real and finite COM/FOM outputs, independent Python vs Octave COM/FOM agreement, fail-closed negatives and required CI. **Completed in run #43972559021**.

**Not complete for `012_09` / v1.4.0:** final approved IEEE Std 802.3ck-2022 Table 120F-8 complete numeric verification, final Test1/Test2 normative parameter lock, native **C++20** IEEE 93A/120F CTLE/TX-FFE/DFE/ISI/XTALK/jitter/noise/equalizer optimization and COM calculation, and at least 3 **same final approved PHY** independent pairs agreeing with **native C++**. The existing C++ `calculateCom()` intentionally still returns `NotEvaluated`. No numerical PHY PASS/FAIL, no native GUI COM activation, and no v1.4.0 release can be based on this historical candidate.

## Failure history and correction evidence

- [First test failure #43869557024](https://github.com/july-rust/SParamView-Public/actions/runs/43869557024): CSV number `19` vs `19.0` test-string artifact, fixed by numeric comparison.
- [Strict OOXML workbook diagnostic #43959294070](https://github.com/july-rust/SParamView-Public/actions/runs/43959294070): `openpyxl` omitted original Strict OOXML sheets; explicit `COM_Settings` relationship and arithmetic verifier implemented.
- [Negative cursor/FOM failure #43967988188](https://github.com/july-rust/SParamView-Public/actions/runs/43967988188): wrong physical port mapping caused negative `A_s` and complex FOM. A diagnostic `|A_s|` trick was **never** used.
- [Octave pre-correction #43969982723](https://github.com/july-rust/SParamView-Public/actions/runs/43969982723): its first script mistakenly truncated complex FOM to a real scalar, identified by `ComplexWarning`. This was fixed with strict real/imag inspection and fail-closed. Correction of the physical source port map then produced real FOM values agreeing with SiCoPR.
- [All source-corrected reference cases #43971752685](https://github.com/july-rust/SParamView-Public/actions/runs/43971752685): 3 Python cases and Octave 10 dB success.
- [Fully crosschecked 3×2 independent reference #43972559021](https://github.com/july-rust/SParamView-Public/actions/runs/43972559021): all six cases passed.

