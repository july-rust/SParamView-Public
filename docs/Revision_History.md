# SParamView Revision History

**Current official release: [v1.3.1](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.1)** — multi-process cache cleanup locking and real-process isolation regression across supported platforms. The [v1.3.0 release](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.0) was published on 2026-10-29 11:43 KST and remains preserved.

This document records the development sequence from the original SI Analyzer engineering preview through the current SParamView release. The development record predates the GitHub repository itself: the first retained engineering-preview validation is dated **2026-09-08**, while the current GitHub repository was created on **2026-09-25**.

Historical entries before v1.1.5 refer to private development records, not files shipped in this public repository. They are historical context, not current validation evidence. v1.2.2 and v1.2.3 were development releases. v1.2.4 was initially promoted publicly; it was subsequently superseded and its public release/tag retired after successful publication of **v1.2.4a**. The follow-up **v1.2.5** improves temporary-data cleanup without changing numerical analysis algorithms. **v1.3.0** extends Touchstone S-parameter input compatibility and relocates Windows auxiliary executables; the existing tagged release assets remain immutable.

## Development timeline

| Revision | Main purpose / change | Primary record |
| --- | --- | --- |
| **0.1.0-preview.1** | Initial SI Analyzer Engineering Preview. Touchstone import, channel-oriented RL/IL/NEXT/FEXT/Quick TDR, result table/plot, limits, export, mapping and large-data cache architecture were established. | `docs/Validation_Report_v0.1.0.md` |
| **0.1.2** | Review-driven correctness and execution improvements: dB sign/limit direction, Channel Mapping validation, Qt worker-pool behavior, responsiveness and real-file regression validation. | `docs/Review_Validation_v0.1.2.md` |
| **0.1.3** | Quick TDR compatibility improvements for difficult real files, direct nonuniform integration fallback, graph navigation/zoom/pan, light-theme consistency and mixed Differential/SE candidate handling. | `docs/Upgrade_Validation_v0.1.3.md` |
| **0.1.4** | TDR termination comparison: Reference, resistive differential/SE, grounded split, Open, Short, and reflection-coefficient display. | `docs/Termination_Validation_v0.1.4.md` |
| **0.1.5** | Performance and display refinement: TDR/trace caching, responsive plotting, display-only simplification, viewport-detail reconstruction and expanded real-file validation. | `docs/SIAnalyzer_Validation_v0.1.5.md` |
| **1.0.0 RC1** | SParamView naming/release stabilization, comprehensive release review, large-file and GUI workflow verification while preserving the validated C++ numerical engine. | `docs/SIAnalyzer_Release_Review_v1.0.0_RC1.md` |
| **1.0.1** | File-open and compatibility fixes. Automatic mapping failures no longer discard otherwise valid S-parameter data; manual mapping remains available. Added stronger P/N and hardening regression coverage. | `docs/Release_1.0.1.md` |
| **1.0.2** | Graph/result-table maximize modes, workspace restoration, selection synchronization and layout validation across common display sizes/scales. | `docs/Release_1.0.2.md` |
| **1.0.3** | UI spacing/boundary cleanup, toolbar/result-table margins and compact workspace usability improvements. | `docs/Release_1.0.3.md` |
| **1.0.4** | Advanced control relocation, English technical UI terminology, locale-stable technical numeric entry and Windows ARM64 preparation/verification work. | `docs/Release_1.0.4.md` |
| **1.0.5** | TDR display-resolution improvements, SE/Differential view separation, viewport-focused detail reconstruction and Windows x64/ARM64 release validation. | `docs/Release_1.0.5.md` |
| **1.0.6** | Touchstone/SNP compatibility hardening, more tolerant validated `.DiffChannels` parsing, conservative polarity handling, cache generation update and release metadata cleanup. | `docs/Release_1.0.6.md` |
| **1.1.0** | Mapping-confirmation safety policy. Explicit metadata is authoritative; name-only P/N mappings remain candidates. Added manual Differential pairing, Swap P/N and Swap Near/Far. | `docs/Release_1.1.0.md` |
| **1.1.1** | Three-state direction control (Near→Far / Far→Near / Both), both-direction overlays, ΔRL/ΔIL diagnostics and channel-to-plot highlighting. | `docs/Release_1.1.1.md` |
| **1.1.2** | Analysis correctness fixes: individual metric execution, endpoint-aware job creation, invalid aggressor suppression and valid bidirectional-delta gating. | `docs/Release_1.1.2.md` |
| **1.1.3** | Revision-state isolation. Quick Analysis results from one SNP no longer leak into another Revision's individual RL/IL/NEXT/FEXT/TDR display; mapping changes invalidate stale jobs/results. | `docs/Release_1.1.3.md` |
| **1.1.4** | Decision-integrity hardening for incomplete frequency/time coverage, limit interpolation boundaries, N/A/NG precedence and observational manual markers. | `docs/Release_1.1.4.md` |
| **1.1.5** | Added Open Project / Save Project header buttons and established the standard three-platform release set: Windows x64, Windows ARM64 and macOS Apple Silicon arm64. | `docs/Release_1.1.5.md` |
| **1.1.6** | Parser/input validation and fail-safe hardening plus TDR Review Fit / Fit All improvements without changing the established numerical algorithms. | `docs/Release_1.1.6.md` |
| **1.1.7** | Refined initial Quick TDR review window to approximately −0.1 ns…1.5 ns for zero-based results, with pre-roll for nonzero starts and deterministic Reset/Fit All behavior. | `docs/Release_1.1.7.md` |
| **1.2.0** | Maintainability refactor: oversized `core.cpp` and `window.cpp` were partitioned by responsibility while preserving the same translation-unit behavior and C++ Reference Engine. | `docs/Release_1.2.0.md` |
| **1.2.1** | Correctness fixes for deterministic result sorting with missing Margin and Quick TDR suitability restricted to the selected time range. | `docs/Release_1.2.1.md` |
| **1.2.2** | Compact RL/IL/NEXT/FEXT/TDR option grouping without reducing graph area, TDR **Option** label, and correct pre-launch macOS application icon packaging. | `docs/Release_1.2.2.md` |
| **1.2.3** | Differential auto-detection upgrade: exact one-character P↔N recognition anywhere in names, Signal Type Detection/confirmation before Channel Mapping for non-authoritative cases, metadata-priority fail-safe, and signal-type persistence. | `docs/Release_1.2.3.md` |
| **1.2.4** | Exact analysis-job reuse, persistent Single-ended choice, full-hash project restore, complete dirty-state handling, Touchstone 2.1 compatibility, report provenance and immutable three-platform publication. | [Release notes](Release_1.2.4.md) |
| **1.2.4a (historical release)** | C++/Qt compile-warning cleanup, native Windows runtime packaging, Node 24-compatible CI, and preserved v1.2.4 numerical calculations. The 2026-10-13 **post-release CI-only** change removed remaining sanitizer/toolchain and Qt test-platform messages, eliminated the Node artifact-download warning, and strengthened ARM64 mapping tests (10/10). Published tag and package digests remained unchanged. | [Release notes](Release_1.2.4a.md) · [CI](https://github.com/july-rust/SParamView-Public/actions/runs/38383938009) |
| **1.2.5 (historical release)** | Per-instance GUI session caches, cleanup after worker/cache-handle release, guarded abandoned-session recovery, Excel staging cleanup and automatic CLI default-cache removal. Originals, projects, exports and UI settings are preserved. | [Release notes](Release_1.2.5.md) · [Cache policy](Temporary_Cache_Cleanup.md) |
| **1.3.0 (previous official release)** | Touchstone 1.0/1.1/2.0/2.1 S-parameter import validation, scoped vendor port-label comments, strict `.DiffChannels` physical mapping, and schema-v7 cache invalidation. Windows keeps `SParamView.exe` as its only root EXE and moves CLI/tests to subdirectories. | [Release notes](Release_1.3.0.md) · [Release](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.0) · [CI](https://github.com/july-rust/SParamView-Public/actions/runs/40807027224) |

| **1.3.1 (current official release)** | Multi-process cache ownership hardening, registry-serialized shutdown cleanup, independent simultaneous Touchstone imports, orphan recovery and real-process regression coverage. No Touchstone parser or numerical analysis changes. | [Release notes](Release_1.3.1.md) · [Release](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.1) |

## v1.2.4a post-release maintenance (2026-10-13)

- **Tagged release:** [v1.2.4a](https://github.com/july-rust/SParamView-Public/releases/tag/v1.2.4a) remains pinned to commit `139b05fb112aec3aba4637a32eec48d1cc796672`. The original 18 release assets and SHA-256 values were not replaced.
- **Maintained source:** PR [#7](https://github.com/july-rust/SParamView-Public/pull/7) was merged into `main` as `48d0233be71629a48704fa9d80fd1dbd1d6c9727`, adding CI/test-only corrections while preserving the published application binaries.
- **Validation:** [Official main Actions](https://github.com/july-rust/SParamView-Public/actions/runs/38383938009) completed all seven jobs successfully. Logs showed no compiler, Qt plugin or artifact-download warnings. All 10 Windows ARM64 fallback mapping checks preserved the eight manually entered columns; source data and numerical algorithms did not change.
- **Document provenance:** previous v1.2.4a guides documented that release; embedded screenshots originate from v1.2.1 test captures and retain their historical provenance.

## Stable engineering principles carried forward

- The C++ implementation remains the production **Reference Engine**.
- RL/IL use signed S-parameter log-magnitude dB; graph labels expose the actual S/Sdd parameter.
- NEXT/FEXT direction is derived from explicit Near/Far Channel Mapping.
- Single-ended 50 Ω, differential P–N 100 Ω and P–GND/N–GND 50 Ω are treated as distinct electrical conditions.
- Quick TDR is a band-limited derived estimate and remains a Preview feature for absolute physical accuracy.
- Valid Touchstone numerical data is kept separate from interpretation: a mapping problem must not unnecessarily make a valid file unreadable.
- Release promotion requires automated common-core regression plus Windows x64, Windows ARM64 and macOS Apple Silicon validation.

## Repository milestone

The GitHub repository `july-rust/SParamView` was created on **2026-09-25**. Earlier 0.1.x records are retained in that private development repository as the pre-repository engineering-development history and form the baseline from which the 1.x product evolved.

Copyright © 2026 july <sparamview@gmail.com>
Licensed under the MIT License.

