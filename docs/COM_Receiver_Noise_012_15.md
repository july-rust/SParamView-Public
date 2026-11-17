# 012_15 — Gaussian receiver noise: bounded mathematical qualification

**Scope:** finite-band, one-sided Gaussian receiver-noise variance/RMS,
PDF/CDF and positive one-sided tail quantile. This is a **NON-NORMATIVE**
diagnostic component, not a complete IEEE 802.3ck Annex 93A statistical
noise model, COM dB, FOM, BER or a PHY PASS/FAIL decision.

## Inputs and units

- Caller supplies a **one-sided PSD in V²/Hz**, an explicit 0-Hz-to-F
  increasing frequency grid, and complex H_noise(f) measured relative to a
  **specified noise injection reference plane**.
- The engine does **not** infer the injection point from a THRU trace or
  automatically add CTLE/channel gain. Upstream must substantiate exactly
  which filtering and reference planes are included in H_noise.
- Example only: existing final 120F-8 configuration stores
  2e-8 V²/GHz = **2e-17 V²/Hz**. The existing fourth-order receiver filter
  may be passed as the explicit transfer for a component-level diagnostic;
  this does not qualify an entire Annex 93A noise chain.
- At most **8,193 frequency nodes** and **256 requested PDF/CDF voltages**;
  no implicit resampling, interpolation or beyond-band extrapolation.
- Existing statistical component accepts one-sided 1e-12 ≤ p ≤ 0.1.

## Mathematical model

In the explicitly finite integration band, a one-sided density is
integrated exactly once:

\[
\sigma^2 = \int_0^F S_n(f)|H_n(f)|^2\,df,\quad
\sigma=\sqrt{\sigma^2}.
\]

Composite trapezoids and compensated accumulation produce V² and V.
For positive RMS, the Gaussian PDF is in 1/V,
CDF is dimensionless, and the exact existing **Gaussian-only** branch of
\`calculatePam4StatisticalNoise\` yields Q such that P(N > Q) = p.
There are **no PAM4 taps** in this receiver-only model. The raw
\`oneSidedTailProbability\` is not automatically equated to IEEE DER.

For zero variance, return a validated degenerate diagnostic and
\`NO_CONTINUOUS_DENSITY\` without a continuous PDF or positive-tail quantile.
Bad PSD, nonfinite values, non-increasing axes, overflows, budgets and
cancellation are fail-closed.

## Verification and acceptance

One focused, Qt-free CTest target
\`channel_com_receiver_gaussian_noise\` verifies an analytic flat filter;
a finite-band first-order lowpass with independent
S*fc*atan(F/fc) oracle; Gaussian PDF/CDF symmetry; known independent
Q^-1(1e-5)=4.264890793922825; PSD/gain scaling, degenerate, adversarial,
budget and cancellation cases. Reuse existing
\`channel_com_statistical_noise\` and
\`channel_com_120f8_final_parameters\` regression tests.

Do not assert passing CI, native OS execution, review or merger until the
final exact HEAD evidence has been retrieved. Keep \`calculateCom()\`
\`NotEvaluated\`. No new IEEE reference audit, external reference batch,
GUI, jitter, NEXT/FEXT PDF, optimizer, main merge, tag or v1.4.0 release.

## Source references

- [Final IEEE 802.3ck profile implementation](../include/si/channel_analysis/com_120f8.hpp)
- [Existing pure-Gaussian and discrete/PAM4 statistical kernel](../src/channel_analysis/com_statistics.cpp)
- [Independent mathematical tests](../tests/channel_analysis/com_receiver_noise_tests.cpp)
- [Published IEEE 802.3ck standard landing page](https://standards.ieee.org/ieee/802.3ck/7322/)
