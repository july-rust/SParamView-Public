# v1.2.4 validation record

## Scope and baseline

- Development source: v1.2.3 `f82b0a202ea0d0c652a227f4c5121b918ad9c8db`.
- Public starting commit: `0dc652014773766a6723045f4e3004e1a8f0315d`.
- Local comparison: Linux x64, GCC 13.3.0, Qt 6.4.2, offscreen. This is separate from the three release platforms (Qt 6.8.3 in GitHub Actions).
- The 33 baseline `src/` and `include/` files were checked against the GitHub tree. Apart from trailing-newline normalization, only `window_validation.cpp` is instrumented for deterministic CSV export and project roundtrip. Baseline calculations were not repaired or retuned.
- Source samples are read-only. Altered regression fixtures live under build/test directories. The seven original source hashes were verified again after export validation.

## Reproduction and regression

| Case | v1.2.3 observation | v1.2.4 coverage |
|---|---|---|
| Directional labels with Single-ended inference disabled | Differential inference incorrectly retained | `v124_named` |
| Additional option line | Rejected instead of ignoring later options | `v124_options` |
| Symmetric mixed-mode Lower/Upper | Unconditionally rejected | `v124_triangle` compares Full/Lower/Upper |
| A-only IL → uncomputed B-only IL | A result was reused | `v124_gui_reuse`, with add/remove, search/group, AUTO/specific Aggressor, mapping, direction and immutable report snapshot |
| Saved checkbox changes | Dirty state not set | `v124_gui_dirty` and `v124_gui_transition` |
| Changed source project restore | Old confirmed channels remained populated | `v124_gui_project_changed` and `v124_gui_roundtrip` |
| Single-ended → Suggest from Labels | Differential inference re-enabled | `v124_gui_single_review`, including cancellation/re-entry |
| Existing full SHA-256 cache primitive | Already correct | `v124_hash`; reused by project restoration |
| Existing non-Apple numeric grammar | Already correct | `v124_numbers`; same corpus runs on macOS and Windows |

[Baseline core output](baseline-core.json) and [baseline GUI output](baseline-gui.log) record the expected failures. They are reproduction evidence, not candidate failures. Initial local Qt download/build failures and an early malformed test fixture were corrected before obtaining these logs; unexecuted tests were never counted as passes.

The local full CTest run passed **40/40** ([log](local-ctest.log)). After the final defensive S-descriptor change, all seven new GUI cases plus the publication safety test were rebuilt/re-executed successfully ([log](local-final-targeted.log)). Publication safety contains six assertions covering another commit's tag, published-release immutability, unpinned drafts, skipped gates, PR publication and provenance mismatch. Platform CI rebuilds the full final source.

Existing core tests retain RI/MA/DB, frequency units, 2-port ordering, references, descriptors, truncated data, cancellation, cache hardening, TDR and termination coverage. Existing workspace tests cover Revision switching and Metric viewing without calculation. New project cases include changed bytes with preserved size/time outside fingerprint regions, changed labels, changed port count, moved identical content, legacy optional fields, save cancellation/failure and clean restoration.

## Seven-file numerical and export comparison

[matrix-audit.json](matrix-audit.json) records each original SHA-256, dimensions, frequency range, labels/mapping checks and parser warnings. All seven files have 1,677 points over 1 kHz–20 GHz. Every complex matrix value and frequency was independently compared with scikit-rf 2.1.0: maximum observed difference **0**. Baseline/candidate matrix bytes and generated mappings were identical.

Physical mappings were reviewed against the original endpoint net labels and exact P/N distinctions; both engines received the same confirmed mapping. The review establishes label consistency and fixed physical endpoints for comparison, not independent hardware truth. All five metrics and both available directions were requested with limits disabled, automatic source impedance, Reference termination and a 0–3 ns TDR interval.

[export-audit.json](export-audit.json) records full analysis-result equality, representative full-resolution CSVs (one trace per metric/direction), all Excel numerical sheets, required report provenance, project save/reopen and unchanged sources. Baseline/candidate CSV bytes are compared exactly. Excel values are compared through openpyxl; timestamps and new provenance are validated separately from numerical sheets. TDR equality is a regression comparison; the independent scikit-rf check covers input S matrices, not an independent physical TDR measurement.

Reproduction helpers: `scripts/validate_release_samples.py` (NumPy, scikit-rf), `scripts/validate_release_exports.py` (openpyxl). Export instrumentation is documented in `MainWindow::validateProject`. Customer source files and complete analysis data are not published in this repository.

## Platform feedback before publication

The first platform run [38342671961](https://github.com/july-rust/SParamView-Public/actions/runs/38342671961) passed Windows x64, native packaged Windows ARM64, 27 core tests, 27 sanitizer tests and the analyzer. macOS passed 35 of 36 deterministic tests but correctly failed `v124_numbers`: `strtod_l` accepted `++1` after one leading plus had been removed. Publication was skipped; no v1.2.4 release or tag was created.

Leading double/mixed signs are now rejected before normalization on both parser backends. The invalid-number corpus includes `++1`, `+-1`, `-+1` and `--1`. This changes malformed-input rejection only; valid decimal parsing and all analysis formulas are unchanged. Windows also executes the actual deployed x64 bundle's project/analysis/Excel/roundtrip self-test in addition to build-tree CTest and the PE audit. The full final commit must pass all platform gates before publication; earlier successful binaries are not substituted.

## Release-platform gates

Local evidence does not establish Windows/macOS package success. Official publication separately requires successful GitHub Actions jobs for core/sanitizers/analyzer, Windows x64 build/test/deployment audit, Windows ARM64 cross-build/audit, **native ARM64 packaged executable tests**, and macOS arm64 build/regression/integration/deployment/self-test.

On macOS four GUI tests are excluded from offscreen CTest and run explicitly with `QT_QPA_PLATFORM=minimal`, including workspace layout at 1440×900. They are not marked PASS merely because of exclusion. ARM64 cross-build `-SkipTests` is build-only; native packaged execution is mandatory before publication.

Consult the official release's `Release_Status_FINAL.json` for the exact commit, successful job URLs and asset digests, and its `Validation_Evidence.zip` for CI logs. These are generated only after all required jobs succeed. Tag/commit, build provenance, executable versions, remote asset sizes and SHA-256 digests are verified before the draft becomes public.

## Limits

Automated layout tests preserve the compact settings area and exercise supported interaction paths; they are not physical-display acceptance. Existing user-guide screenshots remain explicitly labeled v1.2.1. macOS is ad-hoc signed and not notarized. Quick TDR remains Preview. No accuracy claim beyond these tests is made.
