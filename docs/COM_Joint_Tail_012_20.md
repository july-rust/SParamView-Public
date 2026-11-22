# 012_20 — Bounded joint statistical tail (NOT IEEE COM)

**Scope:** C++20 Qt-free, finite-state receiver-voltage mixture of independent PAM4 discrete interferers, receiver Gaussian noise, separately calibrated Gaussian random jitter, and symmetrical Dual-Dirac shift. This is **not** IEEE 802.3ck-2022 Annex 93A COM, FOM, DER-validated noise, PHY PASS/FAIL or compliance.

## Mathematics

For up to six independent equiprobable PAM4 symbols \(L_i \in \{-1,-1/3,+1/3,+1\}\), each with calibrated real voltage coefficient \(c_i\), and an independent equal-weight Dual-Dirac sign \(D\in\{-1,+1\}\), use

\[
X = \sum_i c_i L_i + D A + Z,\qquad
Z\sim N(0,\sigma^2),\quad
\sigma = \operatorname{hypot}(\sigma_{RX},\sigma_{RJ}).
\]

Here \(A\) is the calibrated **peak**, never peak-to-peak, Dual-Dirac voltage magnitude, and \(\sigma_{RJ}\) is the independently calibrated Gaussian **random-jitter-only** voltage RMS. The 012_16 combined RX+RJ output cannot be inserted as RJ input because it already includes receiver noise.

For each enumerated discrete state \(s\), with equal state probability, compute

\[
P(X>q)=\frac{1}{N}\sum_s \frac{1}{2}
\operatorname{erfc}\left(\frac{q-s}{\sqrt{2}\sigma}\right).
\]

Find the unique positive threshold \(q\) for input one-sided tail \(p\in[10^{-12},0.1]\) by bounded bisection. The implementation uses \(\sigma\)-normalized voltages and compensated tail summation; it verifies the resulting tail against the requested probability with relative error no greater than \(10^{-6}\). For variance regression use \(\sigma^2 + (5/9)\sum_i c_i^2 + A^2\). If \(A=0\), no duplicate Dirac state is allocated. Maximum state count is \(4^6\times2=8192\).

**Not modeled:** stochastic dependence between aggressors, real receiver decision adaptation, nonlinear jitter transfer, DFE error propagation, normative COM DER/FOM optimizer, package-parameter qualification, or a standard-compliant physical voltage calibration. These assumptions must never be silently introduced into the API.

## Input provenance and fail-closed rules

Each component is a provenance-bearing `Result<ComJointVoltageEvidence>`. Inputs must share source SHA-256, revision, mapping identity, settings identity, workspace and mapping generations, the same explicit Test1 or Test2 tag, receiver decision plane and voltage calibration. A unique component ID, matching component type, nonempty upstream algorithm/version and explicit independence claim are mandatory. Only the receiver Gaussian component carries `includesReceiverGaussianNoise=true`; all other inputs must be receiver-noise-free. In particular, an upstream 012_16 combined RMS is *not* a valid random-jitter-only component. Malformed or duplicate case fields, invalid numerical scales, missing values, stale source or cancellation reject processing.

These metadata checks are **not cryptographic or physical proof** of calibration or statistical independence. The utility cannot create calibrated volts from 012_14 dimensionless pulses or from 012_17/18 normalized ISI or synthesize the normative COM noise reference from historical candidate 012_13.

Output is always `Warning / NOT_IEEE_COM`, model `EXPLORATORY_INDEPENDENT_PAM4_DUAL_DIRAC_GAUSSIAN`. The paired Test1/Test2 helper requires identical tail targets and comparable receiver plane/calibration between cases before reporting the maximum of the two **exploratory noise quantiles**, not worst-case IEEE COM. Production `calculateCom()` remains `NotEvaluated` with no finite COM or verdict. No GUI or released tag is changed.

## Bounded independent numerical acceptance

Dedicated `channel_com_joint_tail` CTest contains source-free independent `math.erfc` + bisection Golden values, analytical Gaussian-only and variance checks, 4-state PAM4, 8-state PAM4 + Dual-Dirac, 32-state tighter tail, scaling, sign symmetry, legacy PAM4/Gaussian correlation, 8192-state bound, source/case/generation/calibration mismatch, double-count prevention, stale/invalid inputs, cancellation and the production COM gate.

Selected independent values at \(\sigma=0.020\) V, \(p=10^{-5}\):
- Gaussian only: \(q=0.0852978158784565\) V
- PAM4 \(c=0.030\) V: \(q=0.10893359953178733\) V
- PAM4 \(c=0.030\) V plus Dual-Dirac peak \(A=0.015\) V: \(q=0.12056005802997405\) V
- PAM4 coefficients \(0.030\), \(0.010\) V, peak \(0.015\) V, \(p=10^{-8}\): \(q=0.15527837162253227\) V

The numerical values are exploratory mathematical mixture goldens, **not** officially sourced or measured IEEE PHY COM values. Report CTest/CI/Codex/merge status only after verifying the actual runs and final PR head.

## References

- [IEEE Std 802.3ck-2022](https://ieeexplore.ieee.org/document/9999414): standard context, not a claim of joint-tail conformity.
- [012_19 voltage-ratio diagnostic](COM_Voltage_Ratio_012_19.md): explicit non-normative previous boundary.
- [012_16 jitter model](COM_Jitter_012_16.md), [012_09 PAM4/Gaussian primitive](COM_120F8_Reference_Numerical_012_13.md): earlier exploratory calculations.
