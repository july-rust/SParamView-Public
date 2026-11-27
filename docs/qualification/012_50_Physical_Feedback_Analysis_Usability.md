# 012_50 physical test feedback — analysis usability correction

Baseline `0fd86413e4ccf31eb67aec241377ecd3e3d4f4af`, user physical Windows report 2026-11-25 KST. Corrective PR, not an official release. COM stays deferred.

- All analysis windows use screen-bounded initial sizing, scrollable content and collapsible settings; ICN graph selector defaults to cumulative RMS with total and allows all three existing panels. Preserve plot data/CSV. Add interpretation and peak/zero references, and dominant aggressor summary.
- ICN default is an explicitly labeled *assumed* independent-path RSS model (not physical confirmation). Multi-selection works without extra checkbox action. Disabling RSS continues to reject multiple paths. Existing compatibility and numerical checks remain.
- Auto Preview Eye is a separate one-click action with phase/DC required and measured bandwidth, confirmed mapping, matched source/response references, assumed ±200 mV incident NRZ, PRBS7, threshold 0, assumed RX range ±1 V. Rate min(4.5 Gb/s, cap/1.5), cap min(10 GHz, measured max). No claim of actual PHY rate or voltage, BER, mask or compliance. Manual mode unchanged; no silent DC extrapolation/renormalization. Explicit assumption labels in results and CSV/PNG.
- Candidate regression: default multi-path ICN and opt-out fail-closed, compact resize/collapse, auto preview and export provenance. Existing numerical tests preserved. Exact-head CI/review and physical retest pending; no main/tag/release change.

## First exact-head CI and review

`293b02ed4fee19452bf4e074b0754a817202f39d`: CI run 45620303832 passed version, core, Windows x64, ARM64 cross-build and native ARM64 gates. macOS job cancelled with no recorded steps; log retrieval returned 404, so root cause is unknown and no macOS pass is claimed. CodeQL run 45620304009 succeeded but reported poorly documented Eye launch function (alert 715); Codex completed without findings. Extracted the manual settings validation into a documented helper, retained preview/manual isolation, and made assumption marking depend on the exact provenance suffix rather than arbitrary text inside source identifiers. New exact-head CI and review required.
