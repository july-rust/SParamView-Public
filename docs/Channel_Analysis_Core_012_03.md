# 012_03 — Common Channel Analysis Core (implementation / qualification)
**Target:** integration/v1.4.0; official v1.3.5 release unchanged.

## API and contracts
- include/si/channel_analysis/common.hpp and src/channel_analysis/common.cpp implement a Qt-free C++20 core linked against the original si_core.
- SourceSnapshot::capture holds a shared_ptr<const Cache>, validates confirmed physical port mapping and generations, and **rehashes the actual Touchstone source** against Metadata::sha256. An empty or changed source digest fails closed.
- SourceIdentity equality is the caller's stale-completion gate. When SHA, revision, mapping or generations change, results MUST be STALE. GUI worker lifecycle integration belongs to 012_04.
- extract delegates IL/RL/NEXT/FEXT direction/polarity to existing loadTrace. TDR keeps its original conditioning.
- Status supports VALID/WARNING/STALE/NOT_EVALUATED/UNSUPPORTED/INVALID_SOURCE/CANCELLED/ERROR; VALID never implies conformance PASS.
- validateFrequency rejects nonfinite/nonmonotone/oversized source; resample implements rectangular complex-linear interpolation. Missing DC or values outside source band are never extrapolated.
- fft is bounded radix-2, forward sign negative and inverse sign positive with 1/N normalization. impulse requires measured DC, strictly uniform grid and real DC/Nyquist for Hermitian expansion; output is a *circular discrete impulse*, not a measured eye/BER or an instrument TDR.
- cacheKey is deterministic length-prefixed cache *identity* (not a cryptographic digest), including SHA, revision, physical mapping, generation, algorithm/settings/profile/TX/RX, direction, frequency boundaries.

## Limitations
- No ILD, ICN, Eye or COM engine, no GUI, no standard pass/fail.
- No automatic DC extrapolation, no unconstrained FFT/DFT of nonuniform data, no implicit window/causality or error-convergence certification.
- Cache identity is supplied but not a new shared persistent cache store; original lifecycle/cleanup is unchanged.
- Channel-analysis workers and asynchronous Qt integration follow in 012_04.
- Snapshot verification costs O(source size); a source that changes after capture must be treated as a new revision by consumers. Never treat an earlier snapshot as live after project replacement.

## Qualification
Run:
- cmake -S . -B build -DSI_BUILD_GUI=OFF
- cmake --build build --config Release
- ctest --test-dir build -C Release --output-on-failure

Dedicated channel_common_core covers SHA, generation, mapping, true Touchstone direction, DC/nonuniform/out-of-band rejection, complex interpolation, FFT/IFFT and analytic pulse delay, cancellation, cache key. Three-platform CI and existing v1.3.5 CTest are required. No COMPLETE without CI receipts and squash merge.

## References
[Plan](Development_Plan_012_v1.4.0.md) · [Requirements](Channel_Analysis_Requirements_v1.4.0.md) · [Baseline](Channel_Analysis_Baseline_012_01.md)
