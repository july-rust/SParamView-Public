# 012_23 — Victim Self-ISI bounded joint-tail bridge (NOT IEEE COM)

**Scope:** Qt-free C++20 exploratory bridge from signed receiver-equivalent 012_21 pulses through 012_22 validation into 012_20's already-qualified finite-state statistical tail engine. No production COM dB, Annex 93A DER/FOM, PHY PASS/FAIL, GUI or release changes.

## Strict input contract

The API receives the real upstream two-case 012_21 Result and the **current live** pair Provenance. It calls calculateComVictimIsi on the original 13 signed taps; does not trust an externally supplied self-ISI RMS or allow externally supplied PAM4 taps. The caller is responsible for checking the current workspace SourceSnapshot; comparing two stale cached identities cannot prove freshness.

It requires a separate explicit Test1 and Test2 set of receiver Gaussian-only RMS, RJ Gaussian-only RMS and Dual-Dirac peak voltage. Each independently supplied result must have the same source SHA/revision/mapping/generations, **canonical case-specific** settings (common settings with one trailing case=TEST1 or case=TEST2 field), exact receiver plane and calibration identity, and nonempty upstream algorithm/version. The noise-bundle metadata must also match each scaled case's normalization, explicit calibration-source identity and TX/RX equalizer identities. These caller-authored metadata are NOT physical proof of voltage calibration or statistical independence; Sdd21 is not asserted to equal measured Vout/Vin.

The 012_20 solver validates component identity, independence-assumption flags, source reference, unique component IDs, correct receiver Gaussian ownership, and rejects receiver noise double counting. No DFE, NEXT/FEXT aggressor or independently supplied PAM4 tap may be silently mixed into SELF-ISI.

## Mathematical model and limits

Let h_k be the twelve signed noncursor ISI voltage taps and L_k be assumed independent equiprobable ideal PAM4 levels {-1,-1/3,+1/3,+1}. With independent Gaussian receiver and RJ noise Z and symmetrical Dual-Dirac term D A, the **exploratory model** is

\[
X=\sum_{k\ne0}h_kL_k+DA+Z,\qquad
\sigma_Z^2=\sigma_{RX}^2+\sigma_{RJ}^2.
\]

The existing 012_20 engine is limited to six independent PAM4 taps / 8192 discrete states. Keep that bound unchanged. Sort all nonzero signed self-ISI taps by descending absolute magnitude; tie-break by increasing UI offset. Preserve their original polarity. Enumerate at most the first six taps using 012_20. For omitted taps define

\[
B=\sum_{\mathrm{omitted}}|h_k|,\quad |R|\le B,\quad X=X_s+R.
\]

Because |L_k| <= 1, the ideal continuous-mixture upper-tail quantile obeys the deterministic mathematical inequality

\[
q_p(X_s)-B\le q_p(X)\le q_p(X_s)+B.
\]

The selected-mixture quantile is **not** the exact 12-tap quantile if taps were omitted. Return selected q, omitted bound, outward-rounded low/high envelope, selected/omitted UI indices and counts, state count, model/role and each Test identity. Without omitted nonzero taps the mode is EXACT_ASSUMED_MIXTURE; otherwise CONSERVATIVE_BOUNDED_MIXTURE. The two-Test maximum is an exploratory upper envelope, not PHY COM worst-case margin.

**Numerical limitation:** the 012_20 quantile is solved in double precision with a finite tail-residual tolerance and plateau rejection, not a formally certified exact-quantile interval. The mathematical inequality applies to the ideal q. Outward rounding the omitted bound does not independently prove a rigorous end-to-end floating-point enclosure of the exact quantile. Do not assert one.

## Independent mathematical goldens (not PHY reference values)

A separate Python/SciPy mathematical oracle enumerated PAM4 states, evaluated the erfc-based Gaussian tail, and found its root. All cases have RX Gaussian RMS 0.02 V, RJ 0 V, DD peak 0 V and one-sided tail target 1e-5.

| Case | Noncursor taps (V) | Selected q (V) | Omitted bound (V) | Discrete states |
| --- | --- | ---: | ---: | ---: |
| Test1 | -0.02, +0.04 | 0.1323069531212122 | 0 | 16 |
| Test2 | +0.10, -0.08 | 0.2519709465346396 | 0 | 16 |
| 12-tap Test1 | +0.04, +0.01, +0.006, +0.005, +0.004, +0.003, +0.002, +0.001, +0.0005, +0.0004, +0.0003, +0.0002 | 0.1276790914271497 | 0.0044 | 4096 |

The ideal 12-tap envelope in the last row is [0.1232790914271497, 0.1320790914271497] V, before outward rounding. These are synthetic independent mathematical test values; none is an IEEE-published COM channel verdict.

The single dedicated CTest uses actual 012_21 scaling into the bridge, preserving Test1/Test2 distinction. It tests zero/2/6/7/12 taps, signed polarity reversal, stable tie-break, 8192-state DD boundary, source/case/settings/generation mismatches, voltage tampering, RX-noise reuse, calibration/EQ/plane mismatches, cancellation and production calculateCom remaining NotEvaluated. Never mark unexecuted tests PASS.

## Acceptance and references

One new header/source/test, one CMake registration, one technical note and one living-plan entry. No duplication of whole documents across operating systems. Require exact latest PR HEAD Linux Release+ASan/UBSan, Windows x64+ARM64/native package where applicable, macOS arm64, CodeQL, existing COM regressions and **actual substantive Codex review**. Fix findings before protected squash merge into integration/v1.4.0, then verify post-merge CI. Do not touch main, tags or released v1.3.5. If evidence is missing, report PARTIAL, not 100% COMPLETE.

References: [012_20 joint tail](COM_Joint_Tail_012_20.md); [012_21 voltage scaling](COM_Voltage_Scaling_012_21.md); [012_22 self-ISI](COM_Victim_ISI_012_22.md); [IEEE Std 802.3ck](https://standards.ieee.org/ieee/802.3ck/7322/) (context only, not a compliance claim).
