# 012_04 — Analysis Windows / Async Worker Framework

**Branch:** `integration/v1.4.0` (feature PR first) · **Release:** immutable `v1.3.5`

## Implementation boundaries

- The top-level **Analysis** menu contains ILD, ICN, Eye Diagram and COM. All four menu actions are **disabled by default** until the corresponding real numerical engine is verified and registered by a subsequent stage.
- A separate non-modal `Qt::Window` is owned for each feature. A repeat action reuses/raises that window; closing one deletes only that window.
- Each window has source/revision, victim, optional aggressor, direction, frequency range, progress, cancel, diagnostic status and reserved plot/export controls.
- **Run / CSV / PNG remain disabled**. A placeholder is not a computed chart, and `NOT EVALUATED` is not conformance PASS.
- `si::gui::AsyncScheduler` accepts pure value-based work with cancellation and atomic progress. `QFutureWatcher` delivers results only on the Qt GUI thread. Each feature has at most one in-flight job; at most four jobs across the scheduler. The previous synchronous `QtWorkerPool::run` is not used for this new GUI.
- Cancellation, work-generation invalidation, per-window request generation, setting identity, and `QPointer` ensure that old/destroyed receivers cannot publish stale results.
- `MainWindow::refreshAnalysisContext` stores only human-readable source metadata, source SHA, immutable physical mapping digest and channel labels, not a live cache handle. Real numerical adapters in 012_05–012_10 must capture and verify `si::channel::SourceSnapshot` on the worker thread prior to computation.
- MainWindow import/project restoration/revision selection/mapping changes invalidate analysis work. A project replacement first cancels old workers and retains the retiring `QTemporaryDir` until all in-flight jobs drain, then removes it. Shutdown explicitly stops the scheduler before source cache cleanup. As with other cooperative jobs, real engines must perform bounded cancellation checks.

## Verification

`channel_analysis_window_lifecycle` creates all four windows, checks default-disabled engine actions, action reuse, independent close/reopen, event-loop responsiveness while a synthetic job runs, cancellation, stale completion, worker exception, safe window destruction, invalid mapping rejection, and deferred old-workspace deletion. The existing `header_project_buttons` test additionally checks the actual MainWindow Analysis menu.

```sh
cmake -S . -B build -DSI_BUILD_GUI=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Qt offscreen tests do **not** independently certify real Windows or macOS native GUI behavior; native execution remains the separate 012_12 gate. No synthetic fixture is a numerical accuracy oracle for ILD/ICN/Eye/COM.

**Acceptance evidence:** Use the 012_04 PR's required CI logs, review, squash merge receipt, and post-merge integration check. Record unexecuted checks as `UNVERIFIED`, never PASS.

## Next-stage handoff

012_05–012_10 should inject only verified, snapshot-backed numerical jobs and enable their menu actions after implementing/qualifying their typed engines, plots and CSV/PNG provenance. No IEEE/MIPI conformance or BER assertion is permitted at this stage.
