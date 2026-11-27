# 012_16 — bounded RJ / Dual-Dirac receiver-voltage diagnostic

**Scope and status:** A self-contained, Qt-free, **NON-NORMATIVE** local-linear timing-to-voltage diagnostic using the existing 012_15 finite-band Gaussian receiver noise result. It is NOT IEEE 802.3ck Annex 93A jitter/ISI/crosstalk statistical COM, a PHY compliance result, BER, DER or a PASS/FAIL.

## Inputs and units

- A finite positive **symbol rate in baud**; one symbol unit interval is **1/rate seconds**, not 1/bit-rate for PAM4.
- Random jitter **RMS in UI**, modeled as zero-mean normal timing error with standard deviation `sigma_t = RJ_UI/rate`.
- Dual-Dirac jitter **peak in UI**, modeled explicitly as two equal-probability temporal atoms at **+peak/rate** and **-peak/rate**. Peak is NOT peak-to-peak; atom separation is 2*peak. No conversion to a fictitious Gaussian RMS.
- A separately **calibrated receiver-voltage waveform in V**, strictly increasing time samples in seconds, an interior decision time and explicit voltage reference-plane/calibration identifiers. `012_14` unit-symbol pulses are dimensionless and **cannot** be interpreted as voltages without external physical calibration.
- A **usable** exact `012_15` `Result<ComReceiverNoiseData>`, with nonempty source/mapping identity and matching source SHA, revision, mapping, generation and settings identity. Caller is responsible for substantiating noise injection plane, filtering and source calibration; identical metadata alone does not prove physical independence.
- Tail probability in [1e-12,0.1] and optional finite PDF/CDF query voltages (max 128). A maximum of 4097 waveform samples; jitter magnitudes up to 1 UI as an explicit numerical **budget**, not a standard limit.

## Mathematical model

An interior local slope is computed from immediately bracketing calibrated voltage samples; at an exact interior knot, use the two neighboring samples. No extrapolation. This is a **local linear approximation** and not a nonlinear convolved COM pulse distribution.

```text
UI(s) = 1 / symbolRateBd
sigma_t(s) = randomJitterRmsUi * UI
A_t(s) = dualDiracPeakUi * UI
g(V/s) = [V(t_hi)-V(t_lo)]/[t_hi-t_lo]
sigma_J(V) = |g|*sigma_t
A_V(V) = |g|*A_t
sigma(V) = hypot(sigma_J, sigma_RX)
X ~ 0.5 * Normal(-A_V, sigma^2) + 0.5 * Normal(+A_V, sigma^2)
Var[X] = sigma^2 + A_V^2
```

Receiver noise `sigma_RX` comes from the actual bounded `012_15` result: no additional PSD integration or factor-of-two. Independent RJ and RX Gaussian contributions are a **declared modeling assumption**, not an IEEE-certified independence claim. Sign reversal of slope leaves the symmetric voltage-error distribution unchanged.

For sigma>0 the engine evaluates PDF (in 1/V), CDF and one-sided positive tail quantile using normalized Gaussian complementary error functions to avoid `1-CDF` catastrophic cancellation. For zero Dual-Dirac voltage peak it reuses the existing Gaussian-only `calculatePam4StatisticalNoise` quantile code. For sigma=0, the distribution is one/two discrete atoms, and **no continuous density or continuously defined tail quantile** is emitted; diagnostic `NO_CONTINUOUS_DENSITY` is explicit.

For ambiguous units, stale or mismatched provenance, missing calibration, invalid 012_15 result, nonfinite/overflow/underflow and subnormal intermediate quantities, oversized input, non-monotonic time or cancellation, fail closed. The API follows the existing module pattern of throwing `si::Error` / `si::Cancelled` for invalid/cancelled math; caller converts errors to its UI status. Success has `Status::Warning` and `NOT_IEEE_COM`, never normative PASS/FAIL.

## Independent oracle and verification

The bounded `channel_com_jitter` CTest covers RJ-only, DJ-only, zero jitter/zero receiver noise, negative and zero slope, full RJ+DJ+RX, exact Gaussian-only known Q^-1(1e-5), calibration and Test1/Test2 source separation, cancellation, invalid inputs and budgets.

A fixed independently calculated 50-digit analytic reference for the full mixture with `g=1e9 V/s`, `RJ=0.1 UI`, `DJ_peak=0.2 UI`, `rate=1e9 Bd` and `receiverRms=0.02 V` is:
- Gaussian sigma = sqrt(0.0104) V; discrete shift = 0.2 V
- Total variance = 0.0504 V^2
- One-sided positive quantile for p=1e-5 = **0.61888237820898782678 V**
- PDF(0) = **0.57175727625975764927 1/V**
- CDF(0.1 V) = **0.58088396269182394317**

The test also verifies `final8023ck120F8()` timing conversion for 53.125 GBd, RJ=0.01 UI and peak DJ=0.02 UI. Validated **parameter transcription** is not a certificate of a final IEEE statistical receiver model.

## Scope boundaries

No full DFE compensation, equalizer search, NEXT/FEXT PDF, full IEEE Annex 93A 93A.1.6 COM, detector-error target mapping, COM dB or COM PASS/FAIL. No Qt GUI changes, main merge, release or v1.3.5 tag modifications. `calculateCom()` must stay `NotEvaluated`.

## Sources

- [Published IEEE Std 802.3ck-2022](https://ieeexplore.ieee.org/document/9999414)
- [Final C++ Table 120F-8 candidate and filter API](../include/si/channel_analysis/com_120f8.hpp)
- [012_11 published-source audit](COM_8023ck_Normative_Audit_012_11.md)
- [012_15 receiver Gaussian mathematical qualification](COM_Receiver_Noise_012_15.md)
- [Existing statistical Gaussian-only quantile](../src/channel_analysis/com_statistics.cpp)
- [012_16 independent mathematical tests](../tests/channel_analysis/com_jitter_tests.cpp)
