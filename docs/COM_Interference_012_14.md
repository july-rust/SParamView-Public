# 012_14 — COM NEXT/FEXT individual interference pulse diagnostics

**Scope:** independently identified, already-conditioned complex NEXT/FEXT transfer functions to separate, finite-band, phase-preserving rectangular unit-symbol pulses. This is NOT IEEE 802.3ck COM/FOM, stochastic crosstalk or PHY PASS/FAIL.

## Contract
- The new C++20 API consumes ComPreparedData and an explicitly increasing time grid; results preserve each aggressor's ID, NEXT/FEXT type, sign, phase and delay. Values are *dimensionless per unit symbol*, not calibrated voltage.
- Test1 and Test2 can be evaluated together via the typed Com120F8LoadedTestCaseSpectra wrapper. Both must carry matching source identities, types and frequency grids; the outputs remain separate.
- SourceSnapshot, confirmed physical ports, reference impedances, TX/RX package topology and source provenance MUST be checked upstream. This API cannot re-establish physical coupling from a scalar transfer.
- Zero aggressors yield a validated empty diagnostic. Unknown patterns and timing forbid stochastic combination, power sums and RMS claims.
- Limits: 32 sources, 512 times, 32769 bins and 16,777,216 bin-time-source products per call, including BOTH package cases. Existing strict DC, uniformity, Nyquist, time alias, input finite and cancellation checks apply. Peak memory includes at most one temporary aggressor transfer copy.

## Independent validation
The dedicated C++ test uses an independent finite-band analytic reference: for H(f)=g exp(-j2π f τ) with rectangular symbol period UI, p(t)=g/π[Si(2πF(t−τ))+Si(2πF(UI−t+τ))]. Each sine integral is independently evaluated by Simpson quadrature instead of the product's frequency-bin trapezoid. Also check zero, NEXT/FEXT, negative polarity, gain scaling, reordering, package identity, error handling, budget and cancellation. The test's finite-band oracle does NOT validate the full IEEE COM optimizer.

## Reference and release boundary
- Prior external six-case numerical reference candidate: [012_13](COM_120F8_Reference_Numerical_012_13.md) and [PR #71](https://github.com/july-rust/SParamView-Public/pull/71).
- New code deliberately leaves calculateCom() at NotEvaluated; it neither changes the reference manifest nor promotes the 2019/2020 historical-model case to a final-PHY Golden.
- Record actual PR HEAD, required CI and squash-merge evidence before declaring 012_14 complete. No new release or changes to v1.3.5 or main.
