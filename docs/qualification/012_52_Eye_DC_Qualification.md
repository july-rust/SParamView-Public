# 012_52 — Eye Diagram DC Validation, Extrapolation & Numerical Qualification

Status: implementation candidate; CI, independent review and native physical requalification pending. Base integration/v1.4.0 at 331bc4dd16cff804a1f33aadc8e0b4f4fb8b066e. No main/release change; COM deferred.

Policy: effective start <= 10 kHz automatic, >10 kHz and <100 kHz explicit consent, >=100 kHz blocked. Original finite real DC with sparse positive samples retains legacy behavior only with LIMITED continuity; nonreal or strongly inconsistent sparse DC is rejected. Thresholds are engineering policy, not a standard. A large unobserved band can conceal low-frequency poles. No physical accuracy guarantee. Source unchanged.

Local even-real fit from first two positive samples, compare constant/linear/alternate third-point fit and imaginary linear intercept. Absolute spread 0.001, first imaginary 0.01 for sparse case, near-coincident fit ratio limit 1e-6; sparse original ambiguity guard 0.1 discontinuity vs locally flat 0.02. These are conservative provisional heuristics, not instrument uncertainty. No forced DC=1 or 0. For original real DC with stable low-band candidate, preserve within 0.001; unstable but close to first sample (0.01) retained as LIMITED. Sparse extrapolation is limited even below 10 kHz, without frequency popup. Nonfinite inputs rejected by shared validator. Same bounded FFT and periodic frame as existing engine. Width is sampled (1/samplesPerUI), not continuous.

References: https://scikit-rf.readthedocs.io/en/latest/api/generated/skrf.network.Network.extrapolate_to_dc.html ; https://helpfiles.keysight.com/csg/N1930xB/Analyzing/Analyzing_Data_in_the_Time_Domain.html . This implementation is not a reproduction of either algorithm. Tests: analytic gain/identity, existing DFT, PRBS, delay and resource tests; AC-coupled matched discrete comparison, boundary and suspect DC cases. Full analytic model matrix, GUI prompt paths and native checks must be confirmed before completion.

Local git clone unavailable (DNS); exact-head CI and review required. No verified test results recorded here.

Follow-up coverage: analytic delay, RC low-pass, AC high-pass and lossy-delay direct inverse DFT and convolution; actual Touchstone/GUI preview/manual, modal cancel/accept and CSV/PNG provenance. An unavailable model spread is explicitly exported as NOT_EVALUATED. All tests are added, not yet observed passing.
