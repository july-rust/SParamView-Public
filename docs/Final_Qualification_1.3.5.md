# Final independent qualification scope — v1.3.5

Baseline: official v1.3.4, `d0e9c1878ee9f90c30fc56c2e67723b73ad9b972`.
CSV correction: PR #29, original corrected commit `6521342b2ab072d66d18e8bdb0bb733e610ce135`.

The earlier [Linux GUI investigation](Cloud_Work_Qualification_20261114.md)
reproduced 24 changed text fields out of 116 after a real Calc save. The v3
correction preserved all 116 text and 116 port values across two GUI saves;
ODS inspection found zero formula cells with formula evaluation enabled. An
unprotected `=1+2` positive control evaluated to 3. That evidence is scoped to
LibreOffice 25.2.3.2 on Linux. No universal spreadsheet-safety claim is made.

## New reproducible probes

`python tests/run_failure_qualification.py <si_failure_probe> <output> --platform <windows|cocoa|xcb>`

The probe compiles `src/main.cpp` and the actual MainWindow with a test-only
macro. The production SParamView executable does not compile the hooks.
Each scenario is a separate child process with an isolated application identity.
The runner retains command, environment scope, executable SHA-256, source SHA,
Qt/platform identity, exit code, stderr, remaining cache paths, screenshots and
visible-message/OK-button inspection in JSON. Nonzero and expected fatal exits
are distinguished from graceful error handling.

| Scenario | Required observation |
| --- | --- |
| C++ throw before QApplication | exit 1 and stderr; no dialog requirement |
| Invalid Qt platform | nonzero exit with initialization diagnostic |
| Initialization exception / injected bad_alloc | guarded exit 1; native dialog title/message/OK when available |
| Occupied cache path | failure, exit 1, sentinel preserved |
| Deferred startup file-open exception | real production callback guard, exit 1 |
| Deliberate qFatal | diagnostic, nonzero, next-launch cache recovery |
| Unguarded timer exception | nonzero diagnostic; no promise of general event recovery |
| Forced termination after readiness | abnormal exit; next launch removes abandoned sessions |
| Normal start/close | exit 0, no private session/locks left |
| Worker exceptions / injected bad_alloc | join before rethrow; worker pool remains reusable |

`session_cache_lifecycle` and `multi_process_cache_isolation` independently
cover constructor fault points, ownership locks, a killed cache owner, concurrent
active owners, and original source/project preservation. Those tests are not a
substitute for physical OS permission UI or true memory exhaustion.

## Status interpretation

Results are determined from each actual run's `results.json` and CTest log.
This document specifies tests; its presence does not certify execution.
Native dialogs use Windows/Cocoa platform plugins and Qt mouse events on the
visible button, separately from offscreen tests. Full physical desktop/manual
interaction remains UNVERIFIED. Qt fatal aborts need not show a dialog or run
normal destructor cleanup.

Excel: UNVERIFIED — Excel execution environment unavailable. A licensed desktop
installation with supported automation is required. Native Windows/macOS Calc
import/save dialogs and arbitrary callback/real OOM behavior remain UNVERIFIED
unless separately recorded. A passing build does not establish these results.

The release pipeline records its exact commit in each package and refuses to
replace an existing release with a different commit. All required platform gates
and publication provenance checks must succeed before v1.3.5 is called released.
