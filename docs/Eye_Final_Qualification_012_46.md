# 012_46 — Simulated NRZ Eye final qualification candidate

Baseline `de700ae43761e7d6eadc34bd91bd5d36cb7fbd65`, PR #109. Scope: deterministic periodic NRZ, matched equal real Zref, incident voltages, measured DC and coverage of selected cap, complex linear interpolation and ideal zero-above-cap. No external PHY, BER, mask or lab qualification. COM DEFERRED / NOT_IEEE_COM; main and releases unchanged.

## Candidate changes and planned evidence
- Existing engine retained. Reject unrepresentable time/mV grids and nonfinite impulse energy; immutable settings and frame/overlay counts; max-height phase distinguished from threshold-open decision; explicit transfer/victim/direction CSV and PNG. No extrapolation.
- Independent analytic two-tap impulse/pulse/waveform (0.75 + 0.25 one-UI echo): +/-0.2 V, height 0.2 V, width 1 UI at zero threshold and 0 UI at 0.15 V. Nonuniform clipped complex direct inverse DFT and direct circular convolution (N=1024) compared to production; phase width enumerated separately. PRBS recurrence, 127 period, finite extremes and 65536-point boundary.
- Existing real S2P and nonadjacent asymmetric S4P fixture (forward 0.8, reverse 0.5) through cache/mapping/GUI; CSV reverse-to-forward height ratio 0.625; PNG readback, stale and invalid input guards. Existing ILD differential fixture oracle updated for the same asymmetry.
- Numerical tolerances in tests: voltage 1e-10 or 2e-11 V, impulse 2e-12 for bounded direct DFT, width 1e-12 UI, exact-grid time 1e-18 s; these are discrete-model tolerances, not physical accuracy.
- Periodic FFT frame is a truncated/repeated sequence (PRBS7 frame need not be divisible by 127); width is sampled and overlay only a subset. Unknown physical out-of-band behavior and precursor/causality limitations remain.
- Local clone failed DNS resolution. No local build claimed. Actual exact-head CI, CodeQL, Codex and protected squash/post-merge evidence pending. Native interactive qualification remains 012_50. No 100% COMPLETE until all gates are observed.
