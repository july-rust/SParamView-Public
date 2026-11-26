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

## Follow-up: direct analysis window notification
- 38f3759: CodeQL passed; Windows x64 and macOS failed Korean open Eye window (run 45777624953). Windows ARM64 passed; native packaged gate skipped.
- Use explicit manager-to-window language notification, explicit Run and direction bindings, and refresh newly opened windows. Split the failing assertion without removing coverage. Added translations for common analysis diagnostics and export dialog captions. Retest required; exact original failure subcondition not yet proven.

## Follow-up: dynamic result presentation and human-readable report headers
- Render ILD and ICN summary text, ICN contributions and Eye metrics from existing immutable results without re-running the solver on language changes. Static engine diagnostic identifiers and numeric values are preserved.
- ReportOptions carries a GUI-thread language snapshot; summary/detail/graph presentation headers are localized while sheet names, raw-data headers, configuration keys and serialized values stay canonical. English remains the default for existing automation. Full report prose and all dynamic engine messages remain unqualified.

## Follow-up: main graph and workspace display
- Main graph presentation language stored in PlotSnapshot, not solver settings; redraw invalidates only the rendered graph, without changing axes, results or source context. Export graphs use captured ReportOptions language.
- Localized mapping explanatory templates and workspace notice/summary templates. Template substitution is single-pass, so user filenames containing percent placeholders remain literal.
- Remaining source-origin diagnostic codes and unlisted strings still require an end-to-end audit.

## Follow-up: compatibility and diagnostics
- 057cefe passed all platform and native ARM64 package gates plus CodeQL (45780576450 / 45780576419). Publication skipped.
- Fixed translated group/aggressor placeholder serialization: canonical legacy values are stored and selected by index on restore. Localized TDR option by stable object name and startup dialog. Added live results/CSV byte equality and project serialization assertions. Latest commit requires CI; physical 1024x768 language-specific visual inspection, full string inventory, and native OS dialog localization remain outstanding.

## Final qualification candidate: framework dialogs and compact language layout
- a31a3bb passed three-platform CI, native ARM64 package, CodeQL and result CSV byte identity (45784096655 / 45784096650).
- Qt-owned standard controls use a small embedded framework translator; Qt dialogs intentionally use the non-native presentation so app language is not dictated by the OS. OS-owned menus, file-system names and system diagnostics remain outside the app contract.
- Compare table and dynamic TDR labels refresh from canonical sources; report human-readable configuration descriptions are localized, machine keys remain stable.
- Added explicit 984x688 client size and action-visibility checks for all three analysis windows in all languages, plus nine PNG screenshots per workspace test run. No physical desktop or human glyph inspection claimed. Full arbitrary core exception prose is not yet fully translated; exact coverage audit still needed.

## Diagnostic coverage audit
- Static literal scan of supported Touchstone/cache/mapping/legacy metric and ILD/ICN/Eye core paths found previously untranslated application diagnostics. Added 301 source keys for these paths and mapping explanations; explicit prefixes preserve interpolated identifiers. Core strings, exceptions, output schema and numeric algorithms remain unchanged.
- Exact and explicit-prefix display resolution is used at main warning/status, mapping explanations and selected result/graph tooltips. System-provided exception details and composite engine notes may still contain raw English; no claim that all paths are fully covered. Latest CI and screen evidence are required before merging.
