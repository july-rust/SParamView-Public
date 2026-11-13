# Channel Analysis Extension Development Plan (ILD / ICN / COM / Eye Diagram)

> Status: **PLANNED / NOT IMPLEMENTED**. This is a prospective roadmap, not a v1.3.3 capability or a release announcement.
> Baseline: SParamView v1.3.3. Release version for these extensions: **TBD**. Documentation-only change.

## 1. User-approved UI requirement (2026-11-13)

- Add a dedicated **Analysis** top-level drop-down menu to the existing Qt6 menu bar.
- Menu entries: **Insertion Loss Deviation (ILD)...**, **Integrated Crosstalk Noise (ICN)...**, **Channel Operating Margin (COM)...**, **Eye Diagram...**.
- Selecting an entry opens a **new, independent, non-modal top-level analysis window**, not a tab, side panel or overlay within the existing main analysis workspace.
- The original main window and its RL / IL / NEXT / FEXT / Quick TDR views remain intact and usable while extension windows are open.
- Different analysis windows may be open and used simultaneously; each owns its own controls, plot zoom/pan, selection, progress, and export status.
- Clicking an entry for an already-open analysis kind should raise/focus its existing window in the active workspace by default. If multiple instances of the same kind are added later, make the duplication action explicit rather than creating windows unexpectedly.
- Closing a child analysis window must not close the main application or unrelated analysis windows.
- Support Windows x64, Windows ARM64 and macOS Apple Silicon with native window management and menu behavior.

## 2. Menu & window concept

    File | Channel Mapping | View | Limit | Analysis | Tools | About
                                              +-- Insertion Loss Deviation (ILD)...
                                              +-- Integrated Crosstalk Noise (ICN)...
                                              +-- Channel Operating Margin (COM)...
                                              +-- Eye Diagram...

Each window shall contain: source/revision/channel indicator; analysis-specific inputs and validated presets; Run / Cancel / Recalculate; progress and quality/errors; interactive graph and numerical results; and export of results/plot with metadata. Never quietly inherit stale settings/results from a different project.

Implementation proposal: keep the current MainWindow as the project owner; create a dedicated non-modal Qt6 window/controller for each analysis type (e.g. independent QMainWindow or appropriate top-level QWidget). Share immutable source snapshots and calculation services, **not** mutable graph widgets or main-window analysis result tables. Window ownership and async callbacks must use safe lifetime handling (e.g. QPointer or equivalent).

## 3. Input integrity & lifecycle contract

1. Source: use the currently selected, validated Touchstone revision and confirmed physical/differential channel mapping. Present an explicit source selector if multiple revisions are loaded; do not silently guess victim/aggressor identities.
2. Invalidation: when a source is replaced or a project is switched, cancel/retire work tied to its old generation and mark results stale. Do not show old-project results as valid for the new project; optionally preserve a clearly labeled historical view only as a future explicit feature.
3. Concurrency: computing in a separate window shall not freeze the main Qt event loop or block another analysis window. Respect bounded workers, cancellation, safe shutdown and performance limits.
4. Caching: reuse safe immutable source data only with provenance-based keys. Separate cache identity by source hash, mapping, analysis type, profile/algorithm revision, model inputs and relevant settings. Preserve v1.3.1+ cross-process cache isolation, project-switch invalidation and temporary-cache cleanup.
5. Status: incomplete source frequency coverage, non-passive/non-causal or inconsistent S-parameters, missing reference impedance, insufficient aggressors, unconfirmed mapping, or missing transmitter/receiver/standard parameters must produce a clear quality warning or **UNSUPPORTED / NOT EVALUATED** state as applicable. Do not turn an unavailable metric into PASS.
6. Reproducibility: record source file hash, Touchstone frequency range, channel/port identities, analysis settings, algorithm and selected standard/profile versions in every export.

## 4. Feature scope and implementation order

### Phase A — Shared channel-analysis framework

- Extract reusable access to complex transfer and crosstalk responses without changing existing RL/IL/NEXT/FEXT/TDR semantics.
- Define typed data models/results per new analysis rather than forcing waveforms and profile-heavy results into the existing scalar Metric/Result schema.
- Introduce non-modal independent analysis windows, source snapshot handoff, job progress/cancel, explicit quality state and exports.
- Add menu/window lifecycle and cache invalidation tests before enabling numerical metrics.

### Phase B — ILD (recommended first metric)

- Compute IL deviation relative to a documented fitted insertion-loss baseline, and render measured IL, fitted IL and ILD(f).
- Report fit method, interval, fit coefficients/degree, maximum and RMS deviation; when applicable, implement standard-specific weighted ILD FoM as separately versioned profiles.
- Prevent official compliance PASS/FAIL without the precise standard-specific definition, frequency weighting, and qualification.

### Phase C — ICN

- Support explicit victim and one or multiple aggressor selections; reuse complex NEXT/FEXT path extraction with verified port orientation.
- Integrate crosstalk noise using an explicitly selected TX spectrum/amplitude and RX filter/equalizer model; handle independent aggressors and correlation assumptions transparently.
- Display cumulative contribution, individual aggressor contributions and model assumptions; show output units only when the corresponding amplitude model is known.

### Phase D — Eye Diagram

- Start with an NRZ channel-simulated eye from complex Sdd21 and a specified bit rate, idealized TX pattern and documented interpolation/DC/causality/bandwidth assumptions.
- Add configurable TX rise/fall, pattern/PRBS, TX/RX filters, noise/jitter and (later) PAM4 and equalization.
- Plot eye overlays plus reproducible metrics such as eye height, eye width, sampling point and UI; label results **Simulated Channel Eye** without an appropriate full PHY and measurement model.
- Never infer an official BER or MIPI/Ethernet compliance result merely from the visible eye opening.

### Phase E — COM

- Implement as a separately versioned and testable engine with a named IEEE 802.3 clause/annex and locked profile inputs; assess reference COM code, access/licensing and cross-platform redistribution before integration.
- Include specified channel inputs, packaging/Tx/Rx/equalization, noise/jitter and aggressor conventions; retain the exact reference profile and output traceability.
- Cross-check independent fixtures/reference implementation before enabling standard-qualified COM or PASS/FAIL; never apply a universal COM threshold to unrelated link standards.

## 5. Interface and qualification principles

- Existing main screen remains unchanged in purpose; new features are accessed via the top menu, not appended to the main plot/results table by default.
- Inputs needed for a particular analysis are edited **inside that analysis window**, not via global Quick Analysis settings. Project data is shared read-only.
- Separate per-window presets and view state; close/reopen behavior should be predictable. No hidden persistent analysis cache left at application exit.
- Input and standard/profile versions are explicit; standard-unqualified exploratory calculations must be labeled as such.
- Test numerics with analytical fixtures, external/reference results, known bad/missing inputs and repeated-run determinism; don't claim numerical compliance from code compilation alone.

## 6. Acceptance tests (required before shipping)

- Menu: all four entries are visible in the top-level Analysis menu; selecting each opens a distinct independent top-level window; the main analysis area and result/plot state do not change.
- Multi-window: opening ILD and Eye simultaneously works; interacting, minimizing, moving, resizing and closing one leaves the others functional.
- Reuse: choosing an already-open entry focuses that analysis window rather than spawning an accidental duplicate.
- Data changes: adding a revision, changing channel mapping, switching projects and closing/reopening the main window cannot silently reuse stale numerical results or mis-map ports.
- Cancellation: long-running analyses can be cancelled; worker completion after window destruction cannot dereference UI objects; shutdown cleans up temporary resources.
- Failure modes: unsupported profiles, insufficient bandwidth, missing aggressors and corrupted/inconsistent source data show quality/fail-closed diagnostics, not fabricated PASS.
- Portability: GUI/window lifecycle, numerical fixtures and performance checks pass on Windows x64, Windows ARM64 and macOS Apple Silicon; documentation-only changes should not require full native platform builds.
- Documentation: update EN/KO guides, release notes and revision history **when implemented**; this planning document is not feature-completion evidence.

## 7. Korean requirement summary / 한글 요구사항

- 기존 창 내부의 탭/그래프에 추가하지 않고 **상단 Analysis 드롭다운 메뉴에서 선택하면 각 기능이 별도의 독립 창으로 실행**되어야 한다.
- ILD, ICN, COM, Eye Diagram 창을 **동시에 사용할 수 있고**, 원래 SParamView의 RL/IL/NEXT/FEXT/TDR 화면을 가리지 않거나 변경하지 않는다(사용자가 창을 이동/배치 가능).
- 기존 프로젝트의 Touchstone 데이터 및 확정된 채널 매핑을 읽기 전용으로 공유하되 각 분석 창의 입력·그래프·분석 상태는 분리한다.
- 프로젝트 전환, 파일 교체, 창 닫기, 다중 실행 시 오래된 결과 및 캐시 혼용이 발생하지 않도록 안전하게 무효화한다.
- 규격에 필요한 파라미터가 없으면 임의의 PASS를 표시하지 않으며, Eye Diagram은 정확한 PHY 모델이 없을 경우 시뮬레이션 결과임을 명시한다.
- **개발 순서:** 공통 창/분석 프레임워크 → ILD → ICN → Eye Diagram → COM. 실제 릴리스 버전과 일정은 추후 결정.

## 8. Primary repository baseline

- Existing Qt main menu: `src/window_ui.cpp` (File / Channel Mapping / View / Limit / Tools / About).
- Existing complex trace and mapping: `include/si/core.hpp`, `src/trace.cpp`.
- Existing transform/analysis/plot paths: `src/tdr.cpp`, `src/analysis.cpp`, `src/plot.cpp`.
- This is a design plan only. No implementation, tests, builds or releases are claimed by this document.
