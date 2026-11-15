# SParamView Channel Analysis Development Plan — v1.3.5 Baseline

**Updated:** 2026-11-15 (KST)  
**Status:** PLANNED / **NOT IMPLEMENTED**  
**Frozen baseline:** Official **v1.3.5** tag, its release assets and numerical results remain unchanged.  
**First feature release candidate:** **v1.4.0**, subject to complete implementation and qualification; subsequent feature release numbers and dates are TBD.

This plan supersedes the previous v1.3.3-baseline plan. Do not interpret a menu mock-up, commit, CI build or documentation as a completed feature or a published release.

## 1. User-approved UX contract

Add a **top-level Analysis menu** beside the existing menus. Each enabled item opens its **own independent, non-modal top-level window**, never a tab or embedded panel in the existing main workspace.

| Analysis menu entry | Dedicated window | Primary outputs |
| --- | --- | --- |
| Insertion Loss Deviation (ILD)… | ILD Window | Measured IL / fitted IL / ILD(f), deviation metrics |
| Integrated Crosstalk Noise (ICN)… | ICN Window | Individual and combined aggressor noise, assumptions |
| Eye Diagram… | Eye Diagram Window | Simulated eye and qualified eye-height/width metrics |
| Channel Operating Margin (COM)… | COM Window | Profile-specific COM and diagnostics, when fully validated |

- Keep the main RL / IL / NEXT / FEXT / Quick TDR workspace unchanged and usable; allow **all four different windows to remain open simultaneously**.
- Reopening the same analysis type focuses its existing window (no accidental duplicates). Closing one window must never close the main application or unrelated analysis windows.
- Each window owns its settings, source/revision/channel selection, Run/Cancel, progress, error/quality status, zoom/pan, plot, numeric results and export actions. Show the active source and mapping on screen.
- Window behavior must be native and testable on **Windows x64, native Windows ARM64 and macOS Apple Silicon arm64**. Use Qt6 non-modal top-level widgets with safe lifetime-managed callbacks.
- **Never enable a menu item that claims to calculate a metric before the metric's real calculation, inputs, quality checks and regression tests are ready.** If needed, show clearly disabled items in early feature releases.

## 2. v1.3.5 compatibility boundary

Source references: [v1.3.5 release notes](Release_1.3.5.md), [qualification scope](Final_Qualification_1.3.5.md), `../include/si/core.hpp`, `../src/trace.cpp`, `../src/window_ui.cpp`, `../src/window_analysis.cpp`, `../CMakeLists.txt`.

- Reuse validated Touchstone 1.x/2.x S-parameter imports, complex traces, confirmed single-ended/differential channel mapping, NEXT/FEXT extraction and the present Qt worker/cancellation facilities.
- Do **not** change original Touchstone files, existing result semantics or v1.3.5 `.siproject` handling. Any later project-schema extension must support previous files or explicitly fail closed.
- **Mapping CSV v3** is the current export/import contract. Preserve text/port fidelity, backward import of v1/v2 and protection against unsafe spreadsheet reinterpretation. Do not downgrade to v2 to suit new windows.
- Preserve private-session caches, cross-process ownership/isolation, workspace replacement invalidation, abnormal-exit recovery and application-exit cleanup. Existing v1.3.5 fault-probe behavior must not regress.
- A new window holds a versioned, immutable source/port-mapping snapshot. On project replacement, reimport, mapping edit or source hash change, cancel or invalidate old work and mark any retained graph **STALE**. Disallow results from another project/revision being silently presented as current.
- Cache numerical results by source SHA-256, physical port/direction mapping, configuration, profile/version, algorithm revision and relevant TX/RX parameters. Keep bounded work, memory and cancellation; never share mutable plot controls between windows.
- Split existing core numerical services from new typed `ILDResult`, `ICNResult`, `EyeResult` and `COMResult` models. Do not overload scalar legacy `Metric` / `Result` without reviewing compatibility.

## 3. Incremental work and exit gates

| Stage | Deliverable | Must pass before completion |
| --- | --- | --- |
| **A: Framework** | Analysis menu, dedicated non-modal window host, source snapshots, isolation/cancellation and export framework | Real GUI open/focus/close, simultaneous windows, project-switch and cache lifecycle tests |
| **B: ILD** | Fitted insertion-loss baseline, ILD(f) plot, RMS/max deviation and documented fitting conditions | Analytical/golden cases plus independent reference; no unsupported standards PASS |
| **C: ICN** | Explicit victim/multi-aggressor NEXT/FEXT mapping; TX spectrum/RX filter profiles; power integration and contributions | Independent numeric oracle, polarity/direction and source-amplitude tests; no invented mV |
| **D: Eye Diagram** | NRZ channel-simulated eye from complex S-parameters, UI, pulse response and PRBS settings; later jitter/noise/EQ/PAM4 only with verified models | Time/frequency transform checks, DC/spacing/bandwidth/causality warnings, known channels and eye opening tests |
| **E: COM** | Separately versioned IEEE-profile engine, channel/package/TX/RX/EQ and noise/jitter inputs | Named IEEE edition/clause, reference implementation/oracle agreement, standard-specific PASS/FAIL evidence |

**Sequence:** A → B → C → D → E. Stage A is shared infrastructure, not a separate feature marketing release. Prefer several small, fully tested PRs per stage over one large change. Every stage must include implementation, tests, docs and protected integration, with no open incomplete acceptance conditions.

**Release policy:** v1.3.5 remains immutable. **v1.4.0 is proposed for the first production-ready new analysis capability (A+B)**, provided existing CI and all supported platform packaging gates pass. ICN, Eye and COM are then added only through later explicitly qualified feature releases; do **not** promise specific version numbers or imply all four ship in v1.4.0. Use patch versions only for post-release fixes, not to silently bundle unverified new analyzers.

## 4. Numerical definitions and limits

**ILD:** Document whether IL uses positive loss magnitude or signed dB, the selected baseline fit family, weighting, fit frequency range and extrapolation. A generic RMS or peak ILD is exploratory; a standard-qualified weighted figure requires the exact external specification and a reference fixture.

**ICN:** Integrate crosstalk from correctly oriented aggressors using explicit source spectrum/amplitude and receiver filtering. Record independent/correlated aggressor assumptions. If no amplitude model is supplied, report a normalized/conditional quantity, not an absolute millivolt noise or PASS.

**Eye:** Use complex transfer (including phase), not just IL magnitude. Document interpolation, DC extension, impulse/pulse construction, sampling interval, PRBS/bit rate, receiver threshold and any TX/RX equalization or jitter/noise assumptions. Show **Simulated Channel Eye** and quality status unless full PHY-specific models are verified; a visible eye opening is not measured BER or MIPI compliance.

**COM:** Treat COM as a standards-profile implementation, not a universal formula. Bind outputs and limits to a specific IEEE 802.3 edition/clause/annex and reference algorithm version. Review licensing and redistribution before incorporating an IEEE reference codebase. With missing parameters or mismatched reference results, return NOT EVALUATED / UNSUPPORTED—not a fabricated margin or PASS. Ethernet COM thresholds are not automatically MIPI D-PHY thresholds.

## 5. Non-negotiable validation

| Layer | Acceptance |
| --- | --- |
| **GUI** | Four menu definitions and separate host classes; each enabled item opens the correct window; close/reopen/focus/minimize; main results and layout unchanged; concurrent ILD and Eye windows |
| **State** | Project switch, mapping edit, new Touchstone/revision, stale asynchronous callback, cancelled work and window destruction cannot relabel old data as current |
| **Numerics** | Deterministic analytic fixtures, separate reference computations, convergence/frequency-range tests, invalid/non-passive or inconsistent inputs, insufficient aggressors and unsupported profiles produce clear quality states |
| **Compatibility** | Original RL/IL/NEXT/FEXT/TDR values unchanged; old projects import; Mapping CSV v1/v2/v3 regression; cache isolation, cleanup and v1.3.5 native startup/failure probes |
| **Platforms** | Native GUI and packaged-binary tests on Windows x64, Windows ARM64 and macOS arm64, plus existing sanitizer/CI/asset-provenance gates; physical GUI observations and unavailable Excel/LibreOffice checks labeled UNVERIFIED |
| **Delivery** | Code review, complete tests, protected squash merge, EN/KO user guides, release notes and revision history all refer to the exact released commit, tag and verified assets |

Record actual source hashes, trace/profile versions, GUI evidence/logs and exact executed tests; **never write PASS from an unexecuted test or from another OS**.

## 6. Suggested implementation boundaries

- UI: extend `src/window_ui.cpp` with a top-level Analysis menu and a minimal window manager; add dedicated ILD/ICN/Eye/COM Qt window classes. Keep presentation separate from jobs/results.
- Core: add independent `channel_analysis` service contracts for frequency/time-domain transforms, noise integration and standard profiles, reusing `Cache::trace` and existing validated channel data where applicable.
- Background workers: share only immutable input data; each window owns its cancellation generation and asynchronous completion safety.
- Export: every result identifies file hash, channel/pins, input frequency span, assumptions, configuration, algorithm version and evaluation quality/status. Export errors cannot corrupt the original file.
- Documentation: the present document is planning-only. Update existing release notes and EN/KO user instructions **at implementation time**. Do not rewrite historical v1.3.5 release contents or tagged assets.

## 7. Immediate next development task

**Implement and qualify Stage A first, without release or placeholder calculations.** Its first PR should introduce only the independent-window architecture and lifecycle/state tests while leaving legacy analysis paths untouched. Then complete Stage B ILD and perform release qualification before deciding whether the tested build becomes **v1.4.0**.
