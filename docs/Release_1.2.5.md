# SParamView v1.2.5 — Official release notes

**Release type:** maintenance update from v1.2.4a  
**Targets:** Windows x64, Windows ARM64, macOS Apple Silicon (arm64)  
**Release verification:** official GitHub Actions three-platform release gates must succeed before publication.

## Highlights

- **Automatic GUI temporary-data cleanup:** each SParamView GUI instance stores imported Touchstone `.sicache` and unfinished `.sicache.tmp` files in its own temporary cache session. It releases workers and cache handles, then deletes the session on normal close.
- **Report staging cleanup:** Excel report intermediates use the GUI instance's owned cache session and are removed once report generation completes or the GUI exits.
- **Crash leftovers:** future launches identify SParamView-owned abandoned cache sessions and reclaim them after checking their ownership markers and process locks. Live concurrent SParamView instances are protected.
- **CLI cache cleanup:** CLI commands without an explicit cache directory use a command-private temporary cache that is removed after normal completion or handled error. Explicitly supplied cache directories remain user-managed and are never automatically deleted.
- **User data preserved:** original Touchstone files, `.siproject` projects, exported XLSX/CSV/PNG files, mapping/preset files, and application UI/QSettings files are not cleanup targets.
- **Bounded deletion scope:** old v3 shared caches made by pre-v1.2.5 releases are intentionally not deleted because another older instance may still be using them. Abrupt termination may leave a CLI cache until separately cleaned.

## Engineering compatibility

- Preserve v1.2.4a S-parameter numerical calculations, Touchstone parsing, mapping policies, RL/IL/NEXT/FEXT, Quick TDR and presentation behavior.
- Cache files are acceleration intermediates, not substitutes for the Touchstone originals. Reopening a source after exiting SParamView requires reimporting it and can increase first-load time.
- The standard native platforms are unchanged: Windows x64, Windows ARM64 and macOS Apple Silicon.

## Validation gates

The source includes regression coverage for:

1. Real cache import and GUI close/release ordering with open file handles.
2. Multiple live instances and safe isolation; abandoned-session reclamation, ownership marker and symlink preservation.
3. CLI success/error paths and preservation of caller-managed cache directories.
4. Existing v1.2.4a mapping, project restoration, report, numerical and graphical regressions.
5. Release consistency: program 1.2.5, CMake 1.2.5, Windows VERSIONINFO 1.2.5.0, and macOS bundle 1.2.5 / 1.2.5.0.

**Official publication is permitted only when all six release checks pass:** version consistency, core quality, Windows x64, Windows ARM64, native Windows ARM64 packaged-binary, and Apple Silicon macOS. The publishing workflow verifies package provenance and SHA-256 digests before publishing immutable release assets.

## Limitations and disclosures

- Graceful shutdown is required for immediate cleanup. A hard kill, OS crash, or power loss cannot execute process destructors; the GUI reclaims verified abandoned session directories on a subsequent launch.
- Previous-release unleased shared cache files are excluded from cleanup.
- macOS applications use ad-hoc signing and are not notarized.
- Quick TDR is a derived, band-limited estimate rather than an instrument-equivalent measurement.
- Release automation and regression tests do not substitute for independent physical SI measurement acceptance.

See [Temporary Cache Cleanup](Temporary_Cache_Cleanup.md), [English User Guide](USER_GUIDE_EN.md), [한국어 사용설명서](USER_GUIDE_KO.md), and [Revision History](Revision_History.md).

Copyright © 2026 july. Licensed under the MIT License.
