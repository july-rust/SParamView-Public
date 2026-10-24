# SParamView v1.2.5 — transient cache lifecycle

This change does not change the application version or delete user data.

## GUI cache ownership
- Each GUI window/process owns a unique directory inside the Qt application
  `CacheLocation/v3/session-XXXXXX` path.
- The imported `.sicache`, in-progress `.sicache.tmp`, and GUI Excel report
  staging files go inside that unique directory. The original Touchstone file is never modified.
- Graceful close waits for workers to complete, destroys any deferred
  QFutureWatcher callbacks, releases Revision/Job/cache references, then
  removes the owned directory. This matters on Windows where open file
  handles can prevent directory deletion.
- Cleanup also requires an exact SParamView ownership marker; folders that
  merely match the session naming pattern are not deleted.
- A lock-file lease prevents another instance from deleting a live session.
  The registry is locked during session creation and abandoned-session sweep,
  and stale detection based only on elapsed time is disabled.
- After an abnormal termination, a subsequent launch attempts to remove
  sessions whose process-owned lease is no longer active.
- Files not inside session-XXXXXX, including old shared v3 caches produced
  by previous releases, are **not** removed automatically; legacy instances
  cannot provide a reliable live lease, so blindly deleting their files would
  risk disturbing a running old version.

## Retained user data
Original *.sNp/*.ts files, explicitly saved *.siproject files, XLSX/CSV/PNG
outputs, mapping/preset files, and QSettings are outside the owned session
directory and are never targets of cache cleanup.

## CLI
The default CLI cache uses a private command-scoped temporary directory,
removed when the CLI exits. If the caller explicitly supplies a cache
directory, that directory is treated as user-managed and is not deleted.
A forced termination may leave a CLI directory; it is not purged from the
shared OS temporary root without proof of ownership.

## Validation
- `session_cache_lifecycle`: real cache import, cleanup, concurrent leases,
  abandoned-session reclaim, unrelated files preserved, and symlink-target
  preservation.
- Existing `v124_gui_*` cases exercise loading and project restoration
  through the new scoped GUI cache path.
- `cli_session_cache_cleanup`: success, error path, and caller-managed CLI cache preservation.
- Standard core, Linux Qt, Windows x64/ARM64 and macOS CI matrix remains
  required before the change is considered release-ready.
