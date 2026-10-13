# SParamView v1.2.4

Official public release line: [july-rust/SParamView-Public](https://github.com/july-rust/SParamView-Public).
Development baseline: v1.2.3, commit `f82b0a202ea0d0c652a227f4c5121b918ad9c8db`.
Public starting commit: `0dc652014773766a6723045f4e3004e1a8f0315d` (public v1.2.1 line).

## Changes

- Analysis-result reuse now compares the actual requested jobs: source/cache identity, physical mapping, Victim, Aggressor, Metric, Direction and Termination, together with calculation settings. Channel selection, filters and Aggressor changes cannot reuse an unrelated result. Identical completed requests remain reusable; direction-only viewing changes do not recompute.
- Single-ended selection disables every name-based differential inference route and persists through mapping cancellation/re-entry and schema-1 project save/load. Explicit metadata remains separate evidence. Accepting a different mapping signal type requires confirmation.
- Project restore uses the existing full SHA-256 verification facility. Changed source content or invalid stored port mappings clears confirmation and regenerates current candidates without discarding valid data. Same-content moved files and older schema-1 projects remain readable.
- Saved control changes consistently mark the project unsaved; programmatic restoration stays clean. Project switching honors Save, Discard and Cancel, including cancelled and failed saves.
- Mixed-Mode Order classification uses actual S/D/C descriptors. P/N evidence is separate from Near/Far confirmation, conflicting mappings require manual review, and the mapping dialog identifies unmapped ports.
- Touchstone 2.1 additional option lines are ignored with diagnostics. The first option line stays authoritative. Symmetric Full/Lower/Upper mixed-mode representations produce equivalent physical S matrices. See the [IBIS specification](https://www.ibis.org/touchstone_ver2.1/touchstone_ver2_1.pdf), pp. 6 and 23.
- Excel Configuration records analysis-time signal type, confidence, evidence, mapping source/confirmation, source SHA-256 verification and settings, plus separate analysis-completion and report-generation timestamps.
- macOS number parsing and thread joining are explicit source-level portability code. CI no longer rewrites source strings. macOS workspace layout runs as a separate minimal-platform integration test.
- Includes the v1.2.2 compact 74 px settings area and v1.2.3 signal-detection improvements. The C++ numerical engine, English technical UI and established graph behavior are preserved.

## Validation and evidence

The [validation record](validation-v124/README.md) separates reproduced baseline defects, local tests, seven-file comparisons and platform gates. Raw customer Touchstone files are not redistributed. Source SHA-256, dimensions, mapping audit and numerical/export comparisons are retained as compact evidence.

Official publication requires all six GitHub Actions gates to finish successfully: version consistency, core regression/sanitizers/analyzer, Windows x64, Windows ARM64 build/audit, native Windows ARM64 packaged execution and macOS Apple Silicon. A skipped or excluded test is not counted as a passing execution. The macOS layout/navigation/open tests excluded from offscreen CTest run explicitly with the minimal platform plugin.

The release status JSON records the exact release commit, gate URLs and asset SHA-256 values. The validation-evidence ZIP retains relevant CI logs and the seven-file audit. Every downloadable asset has a SHA-256 sidecar.

## Publication policy

- PR and manual CI runs validate only. Official publication is restricted to a successful `main` push.
- The release tag is pinned to the checked-out build commit; package provenance and binary/package versions must agree.
- The historical one-time v1.2.1 documentation-refresh workflow is retired and has no write permission.
- Any existing published release is immutable. Another commit at the requested tag, or an unpinned draft, stops publication. Existing releases and tags are never replaced.
- A failed publication remains a draft. A retry may reuse only byte-identical draft assets from the same commit; differing bytes stop without overwrite. A rebuilt or changed release requires a new version. There is no `--clobber` path.
- Remote uploaded digest/size and tag identity are checked before promoting a draft to the official release.

## Packages and limitations

Windows x64 and Windows ARM64 ZIPs; macOS Apple Silicon arm64 ZIP and DMG; English/Korean guides; README_FIRST; validation evidence; final status JSON; SHA-256 files.

macOS is ad-hoc signed, not Apple-notarized. Quick TDR remains a band-limited Preview. Automated offscreen/minimal GUI tests do not claim physical display acceptance or equivalence to instrument measurements. Seven-file physical mapping checks use original endpoint labels; they are not a new independent hardware measurement.

Copyright © 2026 july <sparamview@gmail.com>. MIT License.
