# 012_27 — Full-Mixture 85-Digit Numerical Oracle (NOT IEEE COM)

**Date:** 2026-11-23 KST. **Target:** `integration/v1.4.0`.
**Scope:** one Qt-free CTest extension, one offline oracle, this note and a single development-plan entry. No production solver, ABI, COM dB, GUI or release change.

## Exact mathematical reference and reproducibility

The existing 012_20/012_26 engine models the independent uniform four-level PAM4 signed taps (up to six), optional equiprobable `+-A` Dual-Dirac shift and independent RX/RJ Gaussian RMS `sigma = hypot(rx, rj)`. The one-sided tail function is

`T(q) = (1 / N) * sum_states [ 0.5 * erfc((q - shift_i) / (sqrt(2) * sigma)) ]`.

For each high-precision reference, Python `mpmath` evaluates `erfc` and the mixture density at **85 decimal digits**, Newton-refines a **separately generated SciPy** root seed, and verifies `abs(T(q) - p_binary64) < 1e-74`. Every decimal floating-point fixture input is converted to its **exact binary64 rational** first, including the PAM4 `1./3.` constants and requested probability. The reference treats their real-number sum and products at 85-digit precision; C++ intermediate rounding remains a separate numerical error.

**Reproduce (offline only):**

```bash
python -m pip install mpmath scipy
python tools/com_joint_tail_high_precision_oracle_012_27.py
```

No Python packages are linked to or required by application/CTest builds. A separate SciPy `brentq` calculation that enumerates all states using `itertools.product` was used as a binary64 cross-check. Neither oracle reads the C++ output.

## Frozen synthetic Goldens

| Case | RX RMS V | RJ RMS V | Dual-Dirac peak V | Signed PAM4 tap amplitudes V | one-sided p | States | 85-digit reference q (V) |
| --- | ---: | ---: | ---: | --- | ---: | ---: | --- |
| A | 0.02 | 0.01 | 0.007 | 0.012,-0.009,0.0075,-0.005,0.003,0.0015 | 1e-12 | 8192 | 0.1813963720094670315386479554859677831141134423678025915817877153758543493343586899 |
| B | 0.02 | 0.01 | 0.007 | 0.012,-0.009,0.0075,-0.005,0.003,0.0015 | 1e-5 | 8192 | 0.1126899214884016856277887781228827684480569273978998654274233501115022718495382001 |
| C | 0.02 | 0.008 | 0.004 | 0.007,-0.003 | 0.1 | 32 | 0.02901429514022381010684710049276341054830172376178702015751733953682616792601231738 |

Values are **synthetic numerical model references**; they are not measured channels, IEEE normative COM vectors or PHY BER qualification.

## What the CTest proves—and what it does not

The existing `channel_com_joint_tail` CTest now calls the C++ solver on each full-mixture input and checks:

1. `Warning / NOT_IEEE_COM`, unchanged `012_20-1` algorithm version, and exact expected state count.
2. The binary64-rounded independent 85-digit root is strictly within `numericalQuantileLowerV < q_ref < numericalQuantileUpperV`.
3. The independently predicted quantile agrees with the C++ point estimate within `2e-11 V`.
4. The previous 012_26 numerical qualification, nonzero interval, probability endpoint ordering and width budget still pass.
5. All existing fail-closed tests, including numerically unresolved discrete-tail plateau, invalid provenance/noise ownership and cancellation, remain active.

**Endpoint implication:** for `sigma>0`, the ideal finite Gaussian-mixture survival function is continuous and strictly decreasing, so `q_low < q_ref < q_high` implies ideal-model `T(q_low) > p > T(q_high)`. The C++ test uses this monotonicity with a precomputed high-precision root; it does **not** execute an arbitrary-precision library at CTest runtime. C++ `std::erfc` endpoint values are additionally checked using the existing numerical qualification.

**Limitations:** the reference is not a formally certified interval arithmetic oracle for every intermediate binary64 operation. The C++ standard does not promise correctly rounded `std::erfc`; even passing on all supported platforms does not establish a directed-rounding numerical enclosure. Output remains `NUMERICALLY_BRACKETED_NOT_CERTIFIED`. Neither assumed PAM4 independence nor caller-declared voltage/TX calibration is physical proof. `calculateCom()` stays `NotEvaluated`, with no IEEE 802.3ck-2022 Annex 93A/120F COM, DER/FOM or PHY PASS/FAIL claim.

## Execution acceptance

Latest PR HEAD must pass related COM regression tests, Linux Release and ASan/UBSan, Windows x64/ARM64 and native ARM64 package gate where applicable, macOS arm64, and applicable CodeQL checks. Inspect substantive review availability and findings, then use protected squash merge to `integration/v1.4.0`; separately inspect post-merge CI. Do not claim `100% COMPLETE` when required evidence is missing. No edits to `main`, historical v1.3.5 tags, GUI, production C++ or release artifacts.

**Repository sources:** `src/channel_analysis/com_joint_tail.cpp`, `include/si/channel_analysis/com_joint_tail.hpp`, `tests/channel_analysis/com_joint_tail_tests.cpp`, `docs/COM_Joint_Tail_Numerical_Bracket_012_26.md`. Reference calculation: `tools/com_joint_tail_high_precision_oracle_012_27.py`.
