# 012_30 — Exploratory Cursor/Tail dB Interval Uncertainty Budget (NOT IEEE COM)

**KST:** 2026-11-23. **Branch:** `integration/v1.4.0`. **Predecessor:** [012_29](COM_Cursor_Tail_Interval_012_29.md), PR #93.

## Scope and contract

This step is a **read-only diagnostic extension** of the single existing `calculateComCombinedTailBridge` replay in `calculateComCursorTailRatioBridge`. It does not add any upstream replay, new statistical engine, normative COM(dB), new GUI, release, runtime Python, or official PHY verdict.

For each Test let `q_l`, `q_u` denote the already-qualified selected-tap finite-precision quantile endpoints and `B >= 0` the already-calculated outward-rounded absolute sum of all omitted signed taps:

- `numericalWidthV = q_u - q_l`: selected-tap **numerical probability crossing** interval width (not a formal real-arithmetic guarantee).
- `omittedTapEnvelopeWidthV = 2 * B`: nominal additional voltage interval width from the conservative omitted-tap envelope.
- `totalNoiseIntervalWidthV = N_u - N_l`: width from the previously returned combined-tail endpoints after all outward rounding.
- `exploratoryIntervalWidthDb = U_dB - L_dB` **iff** a finite upper dB endpoint exists and the width arithmetic is representable.

Every width is `std::optional<double>`. An absent width means **unrepresentable diagnostic arithmetic**, not zero and not an invalidation of a previously valid voltage ratio. These widths must be nonnegative when present. `totalNoiseIntervalWidthV` need not be bitwise equal to the sum of the other widths because outward rounding and cancellation affect the combined endpoints. Test-specific `omittedTapBoundV`, selected and omitted tap counts are retained for auditing.

`ComCursorTailUpperBoundState` has exactly two values: `Available` for positive noise lower bound with a finite upper dB endpoint; `NonpositiveNoiseLower` for nonpositive noise lower bound, where `exploratoryUpperRatioDb` and `exploratoryIntervalWidthDb` are absent. This is **not** a statement about IEEE PHY margins or a certified distribution support bound. The two-Test minimum upper endpoint follows unchanged 012_29 algebra, including availability from only one Test.

The exploratory bridge provenance advances to `012_30-1` for the expanded diagnostic contract. All upstream algorithm/provenance versions are untouched. The existing ratio lower/upper values, source/case/plane/calibration/noise ownership validation, cancellation, status, and min(Test1,Test2) logic are unchanged.

## Independent qualification

Extend the existing Qt-free `channel_com_cursor_tail_ratio` CTest without a CMake change:

1. Independent high-precision Gaussian q reference for sigma 0.02 V and one-sided p = 1e-5 (`q = 0.08529781587845649257 V`) lies inside reported endpoints; both numerical widths are positive and finite, omitted widths are zero, and dB widths exist.
2. An independent long-double sum of the absolute **omitted tap records** verifies the extra `2B` envelope and numerical/total width relationship with a tolerance allowing outward rounding.
3. Common voltage scaling doubles voltage widths while preserving dimensionless dB width; signed-cursor regression remains in place.
4. When only Test1 has `N_l <= 0`, its upper/width are absent and Test2 still bounds the two-Test minimum; when both fail, both upper/width fields are absent without losing finite lower ratios or voltage diagnostics.
5. Existing stale SourceIdentity, cases, decision plane, calibration, invalid noise and probability, zero cursor, cancellation, and extreme safe-log regression remain.

No test may use the computed diagnostic width itself as the sole independent expected value. Synthetic mathematical references are **not IEEE PHY data**.

## Limitations and completion

All successful results remain `Status::Warning` with `NOT_IEEE_COM`, `NUMERICALLY_BRACKETED_NOT_CERTIFIED`, and unchanged production `calculateCom() == Status::NotEvaluated`. The C++ `std::erfc` implementation is not guaranteed correctly rounded on every platform. Caller-declared physical-voltage scales and statistical independence are not independently proven. Numerical widths are **diagnostics**, not rigorous error bars or COM/DER certification.

`100% COMPLETE` requires latest PR HEAD Linux Release, ASan/UBSan, Windows x64/ARM64 with native package gate where applicable, macOS arm64, CodeQL and COM regression, substantive review where accessible, protected squash merge to `integration/v1.4.0`, and verified post-merge CI. Until then report `PARTIAL`. Do not change `main`, official v1.3.5 tags or release assets.
