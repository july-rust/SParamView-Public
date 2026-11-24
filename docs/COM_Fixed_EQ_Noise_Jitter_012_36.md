# 012_36 — bounded fixed-EQ RX noise/jitter candidate (2026-11-24 KST)

Baseline: 796f41c0a543c9e38cfa90aef07161d5c8408a0a (PR #99). **PARTIAL / NOT_IEEE_COM / FINAL PHY REFERENCE BLOCKED** until exact final-head CI, review, merge and push CI are observed.

Reuses 012_33/34 actual source replay, 012_35 optional historical die replay, 012_15 constant one-sided PSD integral and 012_16 local-linear mixture. Two alternative declared receiver-local injection models: RX filter only or CTLE then RX filter. Neither is certified normative injection location. No victim TX/package response or victim scaling on already voltage-defined PSD. Case metadata binds source, settings, declared voltage evidence, noise source and independence declaration. Explicit declarations are not physical calibration or proof of independence. Missing crosstalk is absent, not zero. No aggregate noise calculation; a representation validator rejects overlapping ownership including pre/post DFE, PSD/RMS, raw/combined Gaussian and mixture. DFE is ideal sampled ISI subtraction, not a continuous compensated waveform.

Independent NumPy ABCD and trapezoidal oracle executed before C++ candidate, based on 012_34 documented matrices, 012_35 synthetic matched S8P victim transfer 0.5 exp(-j2pi f 12ps), 2658 points 0–26.57 GHz, FFE [-.04,.10,-.20,.56,-.10], CTLE -20/-4 dB, times 210/430 ps, declared scales 0.2/0.4 V per unit. Use 012_34 documented causal gamma, TX/RX ladder, Ht/FFE/CTLE/fourth-order RX, rectangular symbol excitation and real trapezoid. Noise PSD 2e-17 V²/Hz, first case RX-only, second CTLE*RX. Local 3 point stencil +/- UI/32; actual represented time difference denominator. Pinned relative tolerance 2e-6 for nonzero expected fixture components (no absolute tolerance accepting zero). Expected values:

| Case | Three voltages (V) | Slope V/s | Noise variance V² | RJ RMS V | DJ peak V |
|---|---|---:|---:|---:|---:|
| Test1 | .013941801951709965, .01423926335753524, .01447460824786692 | 452885351.73340636 | 5.291375372119368e-7 | 8.524900738511179e-5 | .00017049801477022358 |
| Test2 | .021496570539206106, .021789620826230417, .021991424621149502 | 420625969.65188146 | 1.353973295092927e-7 | 7.91766531109424e-5 | .0001583533062218848 |

UI/64 independent sensitivity slopes 453352188.2347169 and 421098724.8036124 V/s, respectively. This measures stencil sensitivity, not a certified derivative error bound. The existing 012_15/16 analytic Gaussian and mixture regressions remain unchanged. No new normative equations or source inspection claimed; published profile parameters traced to 012_11/12, source function mapping remains blocked under 012_32. Production calculateCom NotEvaluated. No FOM, DER, COM dB, optimizer, GUI, main or release change.

Qualification candidate only: independent oracle executed, C++ compile and native CI not yet verified at authoring. See PR receipts for final evidence.
