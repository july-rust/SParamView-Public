# 012_17 — Fixed six-tap DFE symbol-pulse compensation

**Scope:** Qt-free bounded **NON-NORMATIVE** fixed-coefficient ideal-decision DFE diagnosis. This is not Annex 93A full COM, DFE optimization, BER/DER, receiver voltage or PHY PASS/FAIL.

## Design correction and authoritative source

Original command proposed using the band-limited impulse samples from `calculateCom120F8BandlimitedPulse` as DFE postcursors. An impulse is not the PAM4 rectangular **symbol** response. Corrected implementation exclusively consumes a `Result<Com120F8RectangularPulseData>` produced by the existing `calculateCom120F8RectangularPulse`. Upstream algorithm, model identity and source provenance must match exactly, or calculations throw `si::Error`.

Published IEEE 802.3ck-2022 Annex 120F.4.1/Table 120F-8 parameter source has been previously audited in [012_11](COM_8023ck_Normative_Audit_012_11.md). Final profile C2C symbol rate is 53.125 GBd; **UI = 1/53.125e9 seconds**. Approved DFE ranges are taken unchanged from `final8023ck120F8()`: tap 1 [0.30,0.65], tap 2 [0.05,0.15], taps 3–6 [-0.04,0.10]. No undocumented coefficient quantization is invented.

## Strict input and numerical model

Request exactly 13 UI-spaced samples at offsets -3..+9 around a **chosen** main decision cursor at index 3; the existing symbol integrator must supply them. The algorithm never guesses a cursor, resamples/extrapolates, or treats unobserved tails as zero. Each sample is dimensionless per unit symbol amplitude; no V or mV is reported.

The Test1/Test2 source settings identity must contain the explicit token `|case=TEST1|` or `|case=TEST2|`. It must match SHA-256, revision, mapping identity, settings identity and workspace/mapping generations of the upstream symbol-pulse result. This is a software provenance contract, not independently certified package physics.

For a selected cursor `h[0]`, define normalized symbol coefficients `a[k] = h[k] / h[0]` at the sampled instants. The six user-provided, range-validated DFE coefficients `b[k]` subtract correctly decided past symbols only: `r[k] = a[k] - b[k]` for k=1..6. The pre-cursors, cursor and observed +7..+9 UI remain unchanged. The coefficient sign convention is explicit; **correct prior PAM4 symbol decisions are assumed**. There is no error propagation or Annex 93A.1.6 optimizer.

The cursor conditioning guard `abs(h[0]) > 1e-10 * max(abs(h[k]))` is a **numerical** guard, not an IEEE margin. Nonfinite, absent, subnormal, unstable or ill-spaced inputs fail closed. RSS for pre- and postcursors is separately accumulated with stable `std::hypot` over observed samples only; postcursor RSS includes all observed +1..+9 UI. A valid DFE coefficient set can worsen residual RSS; no performance PASS is implied.

## Independent example and verification

Constructed synthetic input: precursor [-3..-1] = [0.02, -0.03, 0.01], cursor = 1, postcursor [+1..+9] = [0.40,0.10,0.02,0,0,0,0.04,0,0], fixed DFE taps = [0.40,0.10,0.02,0,0,0].

Closed-form independent expectations: residual +1..+6 all 0, residual +7 = 0.04, postcursor RSS before = sqrt(0.172), after = 0.04, precursor RSS = sqrt(0.0014) before and after.

Dedicated `channel_com_dfe_fixed_taps` test also checks signed amplitude scaling, valid-but-worse DFE, min/max limits, invalid tap values, cross-case contamination, identity mismatches, impulse substitution, off-grid sampling, absent/unstable cursor, cancellation, and **actual existing rectangular-symbol integration** from a synthetic complex Gaussian low-pass transfer before the new DFE kernel. No heavy Python/Octave COM reference matrix is rerun.

## Hard boundaries

Only fixed six-tap sample-domain calculation and its tests are implemented. No new GUI, no FFE/CTLE/DFE search, no probability convolution, noise/jitter/XTALK aggregation, BER, DER, COM dB or PHY PASS/FAIL. `calculateCom()` stays `NotEvaluated`. Official v1.3.5, main, tags and releases stay untouched.

Close this task only after evidence of exact PR HEAD required CI, review disposition and protected squash merge to `integration/v1.4.0`.

## References

- [Published IEEE Std 802.3ck-2022](https://ieeexplore.ieee.org/document/9999414)
- [Audited Table 120F-8 numerical fields](COM_8023ck_Normative_Audit_012_11.md)
- [Source DFE range and rectangular-symbol declarations](../include/si/channel_analysis/com_120f8.hpp)
- [Standalone DFE regression](../tests/channel_analysis/com_dfe_tests.cpp)
