# SParamView Revision History

This document records the development sequence from the original SI Analyzer engineering preview through the current SParamView release. The development record predates the GitHub repository itself: the first retained engineering-preview validation is dated **2026-09-08**, while the current GitHub repository was created on **2026-09-25**.

Historical entries before v1.1.5 refer to private development records, not files shipped in this public repository. They are historical context, not current validation evidence. v1.2.2 and v1.2.3 were development releases; v1.2.4 is the public promotion.

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
| **1.2.4a** | Warning cleanup in Qt/UI, C++ local identifiers and cross-platform release tooling; preserves the validated v1.2.4 analysis engine. | [Release notes](Release_1.2.4a.md) |

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

