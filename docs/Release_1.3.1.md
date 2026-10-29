# SParamView v1.3.1 — Multi-process cache safety

**Release policy:** Official v1.3.1 packages are published only after the required GitHub Actions checks pass on the main branch. This release has its own new tag and assets; the existing v1.3.0 tag, binaries and attached documentation are preserved.

## Changes
- Serialize normal session deletion with the shared lock used by abandoned-session cleanup, preventing concurrent startup and shutdown from removing the same cache directory.
- Fail safely if the registry lock or cache file cannot be released. Do not remove another live instance's directory.
- Add a real multi-process test covering concurrent startup, imports of the same two synthetic Touchstone files, independent normal exit, abrupt process termination and cleanup on a later launch.
- Preserve original Touchstone files, projects, exports and UI settings. Touchstone schema SICACH7 and numerical analysis remain unchanged.

## Scope
This change improves isolation among separate running processes. File transitions *within* a single window still append Touchstone revisions and retain process-scoped caches; their broader UI cleanup is tracked separately in issue #15.

## Required verification (release gate)
session_cache_lifecycle, multi_process_cache_isolation, v124_gui_cleanup, cli_session_cache_cleanup, and all official Windows x64/ARM64, native ARM64, macOS Apple Silicon and core CI gates. Previously published v1.3.0 binaries must remain unchanged.
