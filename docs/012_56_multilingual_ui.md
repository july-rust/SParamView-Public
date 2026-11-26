# 012_56 Multilingual UI — work in progress

Baseline: integration/v1.4.0 at 86308341173e24a1dfad85366e9fb29316eb1208 (012_55 merged PR #119).
Do not merge or release until all 012_56 requirements are satisfied.

## Implemented in this candidate
- English-only Language / English / Korean / Japanese controls.
- Exclusive selection; stable action object names and persistent UI preference with English fallback.
- Main-menu action translation from immutable original text, including Analysis menu and Eye Diagram action.
- Main-window fixed labels/buttons/tooltips, selected index-based combos, tab titles, result table headers, direction and advanced toggle captions, and header project buttons. Mutable user data and analysis statuses are not included in these bindings.
- No changes to numerical engines, QLocale, caches, project formats, or analysis context.
- Workspace regression test for menu selection, translation, 100 cycles, state/shortcut preservation, and preference fallback.

## Remaining
- Complete context-aware catalog, remaining dynamic main window text, dialogs, ILD/ICN/Eye windows, plots, user reports and error paths.
- Audit comparisons of display strings (e.g. currentText) before localizing any controls used as keys.
- Verify Qt/native system dialogs and font fallback, including Japanese glyphs.
- Full translation inventory, technical terminology review and untranslated-string allowlist.
- Cross-platform GUI, 1024x768, export and numerical parity qualification; CI results and physical evidence.
- No test execution or platform PASS is asserted by this source commit.

This is a partial implementation, not a completed 012_56 or a release candidate.

## Follow-up
- Previous head fba6ae5: CodeQL passed; Windows x64 and macOS failed the Korean static UI assertion, Windows ARM64 CI passed. Run 45772454110. The combined assertion is split and explicit main tab/button source binding added; root cause awaits retest.
- Added fixed analysis window controls, guide text, status prefix and selected plot labels with a language property observer. Dynamic result detail strings and dialogs remain incomplete. No qualification claimed.

## Follow-up: dialog and dynamic notice coverage
- f86d2ede passed the three-platform CI and CodeQL (runs 45774724248 / 45774724390); release publication skipped as intended.
- Localize TDR/mapping/export/axis dialog controls, selected file-dialog titles, known notices and status bar messages, and add live-open analysis window state regression. User-entered identifiers and numerical locale remain unchanged.
- Remaining: complete context-aware dynamic analysis diagnostics, report text, native dialogs, all untranslated strings, actual layout and language parity evidence. Do not treat prior CI as qualification of this commit.
