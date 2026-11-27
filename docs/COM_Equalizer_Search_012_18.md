# 012_18 — Bounded COM FFE/CTLE one-step local search with ideal DFE

**Status:** Initial 012_18 implementation merged in [PR #77](https://github.com/july-rust/SParamView-Public/pull/77), exact-HEAD required CI [#44204941042](https://github.com/july-rust/SParamView-Public/actions/runs/44204941042) **SUCCESS** and post-merge push CI [#44207280011](https://github.com/july-rust/SParamView-Public/actions/runs/44207280011) **SUCCESS**. Follow-up input-provenance qualification is subject to its own exact-HEAD CI and review evidence.

## Purpose and scope

This standalone Qt-free C++20 engine minimizes an **exploratory dimensionless
observed residual ISI RSS**. It is **NOT** IEEE 802.3ck Annex 93A optimum,
normative COM/DER/BER, voltage margin, or PHY PASS/FAIL. Production
`calculateCom()` deliberately remains `NotEvaluated`.

Input is a provenance-bearing `Result<ComPhysicalChannelData>` from the
pre-existing historical causal S4P physical pipeline; the exact upstream
algorithm/version and source SHA, revision, mapping, workspace/mapping
generations **must match** the caller's case-specific `Provenance`. An
unqualified, NotEvaluated, absent or mismatched physical Result fails closed
before numerical evaluation. This guards accidental cross-source mixing;
it does not cryptographically prove that a hand-constructed Result contains
genuine bytes. The caller must use `calculateHistoricalCausalC2cFromSource`
(or a properly verified equivalent), recheck live SourceSnapshot generation,
and avoid fabricating provenance. This engine does **not** itself read a
SourceSnapshot from raw traces. One valid 120F-8 FFE/CTLE seed and an
explicit decision instant are required.

## Search contract

- Six independent discrete axes: TX c(-3), c(-2), c(-1), c(+1), CTLE gDC,
  CTLE gDC2. Seed and at most the two nearest steps of each axis: **13
  attempted slots**, no global or iterative search. Invalid neighbors are
  recorded; duplicates are never double counted.
- TX c(0) is always recomputed from `1 - sum(abs(noncursor taps))` and
  must obey the published minimum. Only previously audited final Table
  120F-8 step/range settings are accepted, without arbitrary rounding.
- A separate candidate settings-identity suffix encodes the discrete six
  coordinates. The case token `|case=TEST1|` or `|case=TEST2|` is fixed.
  Cross-case source mixing is rejected.
- Reuse the existing physical Test1/Test2 conditioned complex spectra,
  IEEE TX Gaussian transition, FFE/CTLE/receiver transfer, and real
  measured-band rectangular-symbol integration. Each candidate uses
  exactly 13 UI samples (-3..+9) around **the same given decision time**.
- For normalized symbol samples a[k] = h[k]/h[0], the independent ideal-
  decision six-tap DFE minimum squared residual under continuous published
  tap bounds is b[k] = clamp(a[k], min[k], max[k]), k=1..6.
  **No unpublished DFE coefficient step is invented.** Feed selected taps
  into the already qualified 012_17 fixed DFE path for residual RSS.
- Score is `hypot(precursorRssAfter,postcursorRssAfter)`. Precursors
  -3..-1 and observed postcursors +1..+9 contribute; only +1..+6 are
  corrected. Cursor is the normalization denominator, not a score term.
  Smaller is preferable. Ties within a fixed absolute/relative 1e-12
  comparison retain the earlier deterministic candidate, seed first.
- This score excludes calibrated receiver voltage, DFE error propagation,
  jitter, RX Gaussian noise, NEXT/FEXT statistical combination, and
  official PHY COM performance. It need not correlate with IEEE COM dB.

The published DFE ranges are sourced exclusively from
`final8023ck120F8()`. Search may find an ISI metric that is worse than
the signal-before-DFE metric because the published tap-1 lower bound
is positive.

## Safe execution

The physical input must be the known historical causal model, with
validated 0-Hz, <=10 MHz step and >=26.5625-GHz grid and finite stable
responses. The initial EQ candidate is validated before search. Search
accepts at most 32769 frequency bins; its **symbol-convolution operations**
are conservatively budgeted at 13 candidates x 13 samples x bins
<=6,000,000. Existing physical/FFE conditioning also executes within
the same fixed candidate and bin bounds. No hidden interpolation,
extrapolation, truncation or cursor/phase optimization.

Missing SHA (64 hex chars), revision, mapping, nonzero generations,
duplicate EQ suffix, wrong case, invalid starting EQ, invalid input
grid, invalid timing, cancellation or an invalid initial numerical
result aborts the whole search. Valid neighboring EQ range violations
are tagged `OUT_OF_RANGE`/`INVALID_EQUALIZER`; numerical candidate
failures are listed as `NUMERIC_REJECTED` and do not silently win.
No candidate may produce a normative PHY status.

## Independent numerical qualification

`channel_com_equalizer_local_search` contains a separate closed-form
13-symbol oracle. With precursor [0.02,-0.03,0.01], cursor 1,
postcursor [0.4,0.1,0.02,0,0,0,0.04,0,0], the exact ideal DFE taps
are [0.4,0.1,0.02,0,0,0]. Expected postcursor RSS before =
sqrt(0.172), after = 0.04, full score = sqrt(0.003). The test
independently calculates these values, exercises strict published DFE
bounds, and rejects NaN/invalid normalized cursor.

The integrated fixture independently supplies a synthetic reciprocal,
passive, complex, delayed two-port on a uniform 0..32-GHz frequency
grid. It wraps the data in an upstream-algorithm-identified Result to
exercise the existing physical FFE/CTLE, actual rectangular symbol
quadrature, DFE, deterministic neighborhood selection, both Test cases,
source SHA/revision/mapping-generation/version mismatches, unavailable
upstream Result, an excessive bin count, nonuniform-grid rejection,
cancellation and fail-closed behavior. The fixture is an engineering synthetic model,
NOT official IEEE PHY qualification. No full historical Python/Octave
3x2 reference is rerun.

## Boundaries and references

No new COM GUI, external optimizer, main/tag/release update or change to
`calculateCom()`. Official v1.3.5 remains unchanged; only
`integration/v1.4.0` receives the protected PR.

- [IEEE Std 802.3ck-2022](https://ieeexplore.ieee.org/document/9999414)
- [120F-8 parameter declaration](../include/si/channel_analysis/com_120f8.hpp)
- [Prior 012_17 DFE contract](COM_DFE_012_17.md)
- [This component](../include/si/channel_analysis/com_equalizer_search.hpp)
- [Standalone regression](../tests/channel_analysis/com_equalizer_search_tests.cpp)
