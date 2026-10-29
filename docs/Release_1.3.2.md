# SParamView v1.3.2 — Project cache isolation and active Touchstone filename

**Release targets:** Windows x64, Windows ARM64, macOS Apple Silicon arm64.

**Publication rule:** v1.3.2 packages, checksums and validation evidence are published only by the official successful, complete three-platform GitHub Actions release job on main. Previously published tags/assets are never overwritten.

## Changes
- **Issue #15 — Project replacement:** The Open Project operation imports and verifies all source files in a new workspace-owned cache directory. Only after success does it commit the project replacement, cancel/wait for old viewport workers, release old analysis/cache handles and remove the retired workspace cache. It resets analysis results, compare table, plot axes and navigation/zoom history, channel selection and snapshots.
- **Failure and cancellation:** Invalid/missing source and failed import retain the former workspace; Save/Discard/Cancel remains available for unsaved changes. Reloading the same project also performs full source SHA-256 verification.
- **Revision workflow:** Open Touchstone still appends one or more revisions to the current workspace, preserving comparison and already imported revision caches.
- **Long filenames:** The native window title presents the currently selected source Touchstone's original filename including extension. Revision changes update the title, and the Revision selector tooltip displays the complete source path. The operating system may visually clip a very long title.
- **Project compatibility:** Saved schema-1 .siproject files no longer serialize obsolete per-session cache paths. Older schema-1 files containing that unused field load normally; hash and port-mapping validation are preserved.
- **Multi-process safety:** The independent cache session/lease/ownership model from v1.3.1 remains in force. No unrelated session or user data is cleaned. The numerical S-parameter algorithms are unchanged.

## Verification gates
- **New:** `v124_gui_workspace_switch` covers full A→B→A replacement, isolation/removal of prior caches, stale plot/navigation/compare clearing, active long-name title and full-path tooltip, additive revision import and invalid project rollback.
- **Existing:** `v124_gui_roundtrip`, `v124_gui_transition`, `v124_gui_project_changed`, `v124_gui_cleanup`, `multi_process_cache_isolation`, `session_cache_lifecycle`, `release_publication_safety`, full core/sanitizer suites and Windows x64, Windows ARM64 (including native execution), macOS Apple Silicon validation.

The publication workflow is the authoritative source for actual CI outcomes. No physical hardware measurement acceptance or instrument-grade TDR accuracy is claimed.
