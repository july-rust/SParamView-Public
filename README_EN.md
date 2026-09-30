# SParamView 1.1.4

Windows x64 / ARM64 Touchstone viewer for channel-based S-parameter and Quick TDR analysis.

Extract the complete Windows distribution and run `SParamView.exe`. Open a project
or import Touchstone files, confirm Channel Mapping, then select QUICK ANALYSIS.


## v1.1.4 decision-integrity update

Validated RL/IL/NEXT/FEXT, mixed-mode, termination, and Quick-TDR equations are unchanged. Explicit finite requested ranges that are not fully covered now yield N/A when the evaluated portion passes, while any evaluated violation remains NG. Redundant collinear points in an equivalent frequency-limit line no longer change the evaluation grid. Manual markers at non-source frequencies are labelled as complex-interpolated display values and never change pass/fail.

## v1.1.3 revision-state maintenance update

Completed results are now displayed and reused only for the active revision. The regression suite covers Quick Analysis followed by a revision switch and individual metric selection, preventing stale results from another SNP from appearing in the plot or result table. Mapping changes invalidate the affected revision's completed results.

## v1.1.0 mapping and workspace update

Explicit `.DiffChannels` metadata and validated Touchstone mixed-mode mappings can be accepted automatically. P/N-like names without explicit differential metadata are candidates only and require manual confirmation. The mapping dialog exposes physical port labels, manual differential pairing, P/N swap, and Near/Far swap controls. Quick Analysis and the analysis button row use compact RL/IL/NEXT/FEXT/TDR labels to preserve more plot height.

## Workspace

| Control | Action |
| --- | --- |
| Split View (Ctrl+1) | Show plot and results together |
| Maximize Plot (Ctrl+2) | Expand the plot |
| Maximize Results (Ctrl+3) | Expand the result table |
| Advanced | Show advanced settings; located immediately after Maximize Results |
| Channel List / Analysis Settings | Show or hide their panels |
| Full Screen (F11) | Toggle full screen; Esc exits |
| View > Reset Layout | Restore the default workspace |

Drag panel splitters to adjust sizes. Table Font changes the table font from 12
to 20 pixels. Switching workspace modes preserves the analyzed values, selected
row, and plot view. All built-in menus, tooltips, and messages use English.
Technical numeric fields use a decimal point independently of OS locale.
User-supplied filenames and labels retain their original Unicode text.

## Interpretation

- Return Loss / Insertion Loss show S11/Sdd11 and S21/Sdd21 in dB.
- NEXT: Aggressor Near to Victim Near. FEXT: Aggressor Near to Victim Far.
- OK means PASS against enabled limits; NG means FAIL. N/A is not a pass.
- Quick TDR is a band-limited estimate, sensitive to DC extrapolation, frequency
  spacing, windowing, and termination. Limited quality and invalid impedance
  samples are reported; do not treat them as an instrument measurement.
- Differential P-N 100 ohm and separate P-GND/N-GND 50 ohm loads are distinct.

See `docs/Release_1.1.4.md` for the current verification scope. Windows x64 is
built and tested natively in CI. Windows ARM64 is cross-built, deployed, and PE-
audited in GitHub-hosted CI; native ARM64 execution remains a separate target-
machine validation gate. Interactive native dialogs/DPI and instrument correlation
remain separate manual checks. `Run_Windows_Verification.cmd` runs the bundled
checks on the target Windows machine.

## Build

Use the included CMake project with a C++20 compiler and Qt 6 Widgets, Concurrent,
Gui and Core development packages. The application version is 1.1.4.
