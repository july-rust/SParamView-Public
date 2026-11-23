# 012_26 — Joint-Tail Finite-Precision Probability Bracketing (NOT IEEE COM)

**Target:** `integration/v1.4.0`. **Contract:** one bounded C++20 numerical improvement. Does not enable normative COM(dB), DER/FOM, BER, PHY PASS/FAIL, or edit `main`/release tags.

## Context and root issue

012_20 provided a point estimate q of the one-sided mixture-tail quantile. 012_25 preserved an absolute-sum bound B on omitted signed victim/NEXT/FEXT taps but, for zero omitted taps, represented a floating-point numerical root as a zero-width interval. That confused exact enumeration of the *discrete model* with exact numerical root evaluation.

012_26 leaves the 012_20 point-estimate fields and upstream algorithm identity `012_20-1` intact for consumers such as 012_23. It extends the returned record with separately qualified numeric endpoints and upgrades the combined bridge's provenance to `012_26-1` because its interval semantics changed.

## Mathematical model and honest qualification

For independent uniform levels {-1, -1/3, 1/3, 1} on at most six signed voltage taps, optionally a symmetric Dual-Dirac peak, and independent Gaussian RX+RJ RMS sigma,

`T(q) = mean_state[0.5*erfc((q-shift)/(sqrt(2)*sigma))]`.

The existing normalized solver obtains `q_est`. A bounded-expansion local check computes distinct `q_low < q_est < q_high` and requires

`T_double(q_low) >= p + p*1e-9`,
`T_double(q_high) <= p - p*1e-9`.

The normalized expansion begins at `1e-8*max(1,|q_est/sigma|)`, permits no more than ten attempts, and rejects a width greater than `1e-5*max(1,|q_est/sigma|)`. Both endpoints are expanded outward by one representable double during multiplication by sigma. Existing checks for the local one-sigma tail slope, tail residual, state budget, source provenance and cancellation remain active.

This **is not a rigorous probability root enclosure**: the C++ standard does not guarantee directed correctly rounded `std::erfc` on all supported platforms. The output identifies itself as `NUMERICALLY_BRACKETED_NOT_CERTIFIED`. Users must not convert it into a certified physical BER/DER or IEEE 802.3ck compliance statement.

For the combined tap bridge, let `B` be the existing outward-rounded sum of absolute values of all omitted taps. Then

`combined_low = outward_down(q_low - B)`,
`combined_high = outward_up(q_high + B)`.

No omitted taps: `NUMERICALLY_BRACKETED_ASSUMED_MIXTURE` and a **nonzero** numerical width. Omitted taps: `CONSERVATIVE_NUMERICAL_BOUND`, with the original identities, signs and UI offsets preserved. The deterministic maximum of the two Test upper bounds is only an exploratory diagnostic.

## Independent qualification

Independent high-precision mpmath Gaussian inverse-CDF references (80-digit computation; constants rounded for C++ fixtures):

| p (one-sided) | RX Gaussian sigma | q reference V |
| --- | ---: | ---: |
| 1e-12 | 0.02 V | 0.14068967650602263860 |
| 1e-5 | 0.02 V | 0.08529781587845649257 |
| 0.1 | 0.02 V | 0.02563103131089200934 |

Existing independent synthetic PAM4 / Gaussian + RJ + Dual-Dirac Golden cases at p=1e-5 and p=1e-8 remain. Regression validates each reference lies inside the computed nonzero interval, endpoint-tail probability ordering, the 8192-state budget, exact included-tap cases, all omitted-tap identities, and the existing independent long-double eight-tap full-enumeration oracle. The previous +/-1e-10 V margin on the eight-tap combined bound check is removed.

Negative regressions retain the deliberately nonidentifiable discrete-tail plateau (p=0.0625), stale inputs, invalid noise ownership, physical-plane mismatch, nonfinite input and cancellation.

## Scope restrictions and acceptance

Only the two current numerical algorithms, two existing Qt-free CTest sources, this document and one development-plan entry change. No new FFT, Monte Carlo solver, runtime mpmath dependency, GUI, production calculateCom() activation, platform-specific documentation passes, tag or release changes.

`100% COMPLETE` requires current PR HEAD Linux Release/ASan/UBSan, Windows x64/ARM64 (including native packaged gate where required), macOS arm64, CodeQL, relevant 012_20–012_25 regression, substantive applicable code review, protected squash merge to `integration/v1.4.0` and post-merge checks. Where this evidence is unavailable, the result is `PARTIAL`, not an asserted PASS.

**Sources:** repository `src/channel_analysis/com_joint_tail.cpp`, `src/channel_analysis/com_combined_tail.cpp`; earlier `docs/COM_Joint_Tail_012_20.md` and `docs/COM_Combined_Tail_012_25.md`. Gaussian references from independent mpmath inversion of the standard normal survival function; synthetic mathematical references are not external PHY channels.
