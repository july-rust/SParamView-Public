# SParamView v1.3.4 — Mapping CSV security and guarded GUI startup

**Targets:** Windows x64, native Windows ARM64 and macOS Apple Silicon arm64.

**Publication rule:** This release is published by the official GitHub Actions workflow after protected `main` integration, full three-platform validation and SHA-256 asset-provenance checks. This note alone is not runtime validation evidence.

## Changes from v1.3.3

- **Stage 2 / PR #25:** Mapping CSV v2 adds an explicit `SParamViewCSV` text-format marker and escaping for spreadsheet formula-leading values. Preserves Unicode, quotes, commas, CR/LF and repeated round trips. Unknown version markers are rejected; unmarked legacy CSV retains its prior apostrophe and default-field semantics.
- **Stage 3 / PR #27:** GUI startup errors receive actionable diagnostics. Automated/headless invocation fails non-interactively with nonzero status. The cache-session constructor retains pending temporary directory/owner lock in local RAII resources until successful initialization, cleaning safely under registry locking after failures.
- **Stage 1 / published v1.3.3:** Keeps Touchstone 1.x/2.x S-parameter parser validation, physical endpoint/port label handling and existing differential mapping policies.

## Compatibility and limits

- No intended changes to electrical RL/IL/NEXT/FEXT/TDR calculations, saved `.siproject` schema, original Touchstone data or user configuration.
- Published [v1.3.3](Release_1.3.3.md) is an earlier immutable source-and-binary release. Its assets are not replaced.
- Mapping CSV v2 text escaping is defense in depth, not proof of formula safety after Microsoft Excel/LibreOffice save/reopen. Spreadsheet application round-trip is **not verified** by C++ tests.
- Interactive dialogs on physical desktops, Qt fatal aborts, OOM and arbitrary asynchronous callback exceptions are not guaranteed by headless regression tests.

## Qualification

The official release requires green Core Release/ASan/UBSan checks, Windows x64 GUI tests and package, Windows ARM64 build plus **native packaged-binary regressions**, macOS arm64 GUI tests and package, publication checks and commit-matched SHA-256 checksums.

Key regressions: `v133_parser_hardening`, `v133_csv_roundtrip_security`, `v133_startup_failure`, `session_cache_lifecycle` plus all historical suites. Actual successful run IDs and checksums must come from the published Actions run, not earlier separate candidate PR checks.

See [Stage 2 assessment](V133_Stage2_CSV_Validation.md), [Stage 3 assessment](V133_Stage3_Startup_Validation.md), [Revision History](Revision_History.md), [한국어 사용 설명서](USER_GUIDE_KO.md), and [English User Guide](USER_GUIDE_EN.md).
