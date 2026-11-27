# 012_58 — Final multilingual integration and release candidate qualification

Status: partial candidate; not RELEASE READY. Baseline integration commit d98bd7c75085d7fbb97dd0252f79eaf474282c90 (PR #122). Keep #121 open.

## Baseline evidence
- PR #122 final head 34a49ba2a00ab7d35c4c3cce02ebba1ff8a0b330: three-platform CI 45908656297, CodeQL 45908656273, COM regression 45908656281 all successful. Publication skipped.
- Three-platform job details show x64 package GUI, native ARM64 packaged regression and GUI, macOS packaged self-test/ZIP/DMG and GUI, and normal/ASan/UBSan core tests.
- Native Windows ARM64 packaged-binary gate job 140216049404 logs show main window min=1220, right panel min=963 and workspace-bar min=943, with left panel min=225. This is a real compact-layout defect, not a physical display certification. Nine previous feature-window screenshots at 984x688 do not qualify the main workspace.

## 012_58 candidate changes
- Main header, workspace toolbar, graph controls and result controls use horizontal overflow strips; rows retain natural control widths and permit scrolling rather than forcing the top-level minimum width. Scrollbar extent is reserved for vertical accessibility. No numerical engine changes. Header project action order and idempotence retained.
- Add 984x688 logical client-area regression with loaded synthetic project and completed analysis, all three UI languages, control reachability, unchanged results/settings/jobs and screenshots. Existing large-window regression is retained. This is a virtual/logical layout check, not physical high-DPI qualification.
- Localize project and Touchstone file-dialog titles/filters; preserve file patterns. Correct platform name in About (Windows/macOS instead of always Windows).
- Independent connector-side catalog audit: 932 keys; 0 duplicates, blanks or placeholder mismatches. Bounded check of 43 explicit lookup occurrences in four changed production files: 0 missing keys. This is not exhaustive runtime or implicit-widget coverage.

## Remaining release gate
- New exact-head build/CI, regression screenshots and CodeQL must pass. Source edits are not locally compiled; git clone failed DNS and no Qt development kit is available in this session.
- Physical Windows x64/ARM64/macOS Apple Silicon display and actual high-DPI tests remain NOT VERIFIED. Horizontal scrolling is a fallback, not a claim that every control is simultaneously visible.
- Representative original S36P files are present in the existing project Library; raw-byte materialization returned no authorized path for either. Do not publish or duplicate user measurements. Previous 012_54 numerical evidence is not a substitute for testing the final packaged binary with these inputs.
- Final merged-commit packaged-binary validation, complete dynamic localization inventory, and remaining #121 gate review are open. Do not close #121, tag, publish, change main, or mark 100% COMPLETE based only on CI.


## Follow-up: exact first candidate CI and vertical compact layout
- Head cee50661d06514d0d97890fad9e1d60174091f6d: CodeQL 45913526013 passed; CI 45913525972 failed. Version and core quality gates (including sanitizers) passed; Windows ARM64 cross-build passed; Windows x64 and macOS GUI failed at new compact size assertion. Windows x64 88/89 tests passed. Native ARM64 packaged gate and publication skipped.
- Both GUI logs show minimum width 661 and actual width 984 in Korean, versus baseline native ARM64 minimum width 1220. Baseline min height 707; new height exceeded target 688 but first candidate logs did not emit exact height. Do not claim compact layout pass.
- Compact strip button padding, workspace margins/spacing, tab padding, result table minimum and readout height reduced; plot minimum size and font retained. Expanded two-dimensional diagnostics and control geometry on failure, require positive plot area, retain 984x688 assertion.
- New exact head requires CI and review. Physical display/High DPI, private original S36P and complete runtime inventory still unverified. Keep #121 open and do not publish.
