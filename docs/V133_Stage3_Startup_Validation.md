# SParamView — Stage 3 GUI startup and cache initialization (unreleased candidate)

**Status:** Stage 3 development branch only. This change is **not included** in the already published v1.3.3 binaries. Do not move or replace the existing v1.3.3 tag, assets, checksums or release metadata. A separately versioned integration release is required.

## Verified source paths (not runtime claims)

- `src/main.cpp` previously constructed `MainWindow` without a surrounding exception boundary. A throwing `CacheSession` constructor could therefore propagate out of startup.
- `src/cache_session.hpp` previously stored the temporary directory as a member before the ownership marker and lease had been acquired. During construction failure, member unwinding could run after the local registry lock had been released.
- `tests/session_cache_tests.cpp` already exercised isolated instance caches, post-close cleanup, abandoned-session recovery and preservation of source/project/settings/export files.
- The existing `cache.cpp` mapped-file format, source fingerprints, lock for cache import and error recovery are not changed by Stage 3.

## Stage 3 candidate behavior

1. Startup exceptions are caught while `QApplication` is alive. Interactive GUI launches show an actionable error dialog and write a diagnostic to stderr. Non-standard exceptions receive a generic message.
2. Failures before a valid QApplication exists are reported only to stderr; no Qt dialog is attempted.
3. `--selftest` and `--validate-project` launches are noninteractive. `offscreen` and `minimal` GUI platforms also suppress dialogs; all startup failures return a nonzero exit status.
4. `CacheSession` now holds its in-progress directory and owner lease in constructor-local RAII objects. Both are moved into members only once construction succeeds. On a thrown exception, they unwind before the registry lock is released.
5. No fallback to a shared cache and no deletion of original Touchstone files, projects, reports or preferences is introduced.

## Tests and evidence requirements

- `v133_startup_failure`: automated CLI detection; dialog suppression for automation/offscreen; failed-startup return codes.
- `session_cache_lifecycle`: occupied-root fail-closed behavior; a real registry-lock contention; fault-injection at four *test-only* constructor boundaries; reuse after failure; existing ownership, multi-instance, cleanup, and preservation cases.
- Native Windows ARM64 packaged validation now runs `si_v133_startup_tests` alongside existing cache regressions.
- The test-only `SPARAMVIEW_CACHE_TEST_FAULTS` CMake definition is scoped to `si_session_cache_tests`; it is not built into `SParamView.exe`.
- Mandatory gates remain CTest/core sanitizer, Windows x64, native Windows ARM64, and macOS Apple Silicon arm64. Record actual run IDs, logs, revisions and measured outcomes on the PR, not guessed PASS counts.

## Limitations

- These changes do **not** guarantee recovery from native crashes, Qt fatal aborts, OOM termination, or unhandled exceptions thrown within arbitrary Qt callbacks/worker threads after startup.
- Interactive modal dialog behavior on a physical desktop is not proven by a headless unit test. Record any actual native/manual UI test separately.
- Filesystem-level permission failures are OS-specific. Occupied-path and injected-failure tests are deterministic coverage, not proof that every OS permission error was reproduced.
- If the registry lock is unavailable, the application still fails closed rather than borrowing or destroying another process's cache.
- Do not merge this candidate into `main` at an unchanged published version if the release workflow would conflict with the existing immutable v1.3.3 tag.

See `docs/Temporary_Cache_Cleanup.md` and `.github/workflows/release-3platform.yml`.
