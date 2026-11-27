# 012_49 — Full application integration and regression qualification

2026-11-25 KST. Baseline: `cf894e269a54215f542da296041f7898bd35b745`, integration/v1.4.0. [012_48 completion](https://github.com/july-rust/SParamView-Public/pull/112#issuecomment-7159731560). No main, tag, release or COM research changes.

## Evidence matrix (candidate; not yet qualified)

| Scope | Existing test / new evidence | Status |
|---|---|---|
| A: analysis, export, project roundtrip | workspace_layout: 012_49 MainWindow scenario; existing golden and channel engine tests, legacy export tests | PENDING |
| B: multi-file, mapping cancellation, independent revisions | workspace_layout: 012_48 stage; new two-source analysis and revision switch; v124 GUI regressions | PENDING |
| C: New, cache retirement, restore | workspace_layout: 012_48 save/discard/cancel; new synchronized verified-source worker reset and reload; session/multi-process cache tests | PENDING |
| D: error and recovery | workspace_layout: failed save, invalid file, per-file cancellation, failed feature CSV export and subsequent recovery; existing parser, CSV and startup tests | PENDING |
| E: cross-feature consistency | real MainWindow menu actions, three distinct analysis windows and actual workers; legacy result preservation; SHA/revision CSV provenance; stale export rejection; ILD through-loss oracle | PENDING |
| Preferences and workspace | existing 012_47 and workspace UI checks | PENDING |
| COM freeze | no action or window through real MainWindow; existing COM research tests unchanged | PENDING |

New deterministic fixture: two single-ended two-lane S4P networks, 0–12 GHz; distinct through amplitudes 0.8 and 0.5; ILD CSV loss expected from -20 log10(amplitude) with 1e-8 dB tolerance (existing channel GUI oracle tolerance). ICN and Eye use existing engine qualification criteria; no new normative compliance claim. Existing core and GUI numerical regressions must remain green. New workspace marker is emitted only after the integration function returns successfully. No new product algorithm or dependency introduced.

Local git checkout unavailable (github.com DNS resolution failed); use actual PR-head CI. Do not mark qualification complete from source inspection alone. Capture actual per-platform test counts, GUI marker, CodeQL, Codex review and protected squash merge; validate actual post-merge push CI and CodeQL on the exact merged SHA. Report remaining gaps explicitly (including actual Excel/LibreOffice, physical file dialogs, multi-monitor and package size qualification in 012_50). No release publication in this stage. If any in-scope scenario is untested or fails, status remains NOT COMPLETE.

## First CI and review correction

PR head `aed1cf05003f4622b364ffacb9a49f3b9a1a9244`: [CI](https://github.com/july-rust/SParamView-Public/actions/runs/45596334026) failed on native macOS GUI timeout after 84/84 ordinary CTest cases passed. Windows x64, ARM64 and native ARM64 packaged gate passed; the native ARM64 log emitted the 012_49 marker. [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/45596334175) passed. The relative fixture path could be resolved a second time against the project directory on macOS; make it absolute and fail visibly on an unexpected modal. The exact timeout cause is not yet confirmed by rerun. [Codex P2](https://github.com/july-rust/SParamView-Public/pull/113#discussion_r4693641458): replace nondeterministic Eye launch/reset race with a separate bounded real SourceSnapshot lifetime probe using the production scheduler; assert acquisition, retained directory while held, successful source extraction after reset and removal after drain. Actual numerical Eye runs still execute independently. These changes are candidates, not confirmed PASS until new CI completes.
