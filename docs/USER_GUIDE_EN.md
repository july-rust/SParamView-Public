# SParamView User Guide

**Applies to:** SParamView v1.2.1 public release  
**Supported platforms:** Windows x64 / Windows ARM64 / macOS Apple Silicon (arm64)

[한국어 사용자 설명서](USER_GUIDE_KO.md) | [English README](../README_EN.md) | [v1.2.1 Release Notes](Release_1.2.1.md)

---

## 1. Introduction

SParamView is a desktop tool for opening, analyzing, and visualizing Touchstone S-parameter data by logical channel. Its primary workflows include:

- Return Loss (RL / S11 / Sdd11)
- Insertion Loss (IL / S21 / Sdd21)
- NEXT (Near-End Crosstalk)
- FEXT (Far-End Crosstalk)
- Quick TDR
- Single-ended / Differential channel mapping
- User-defined limit evaluation with OK / NG / N/A results
- Revision comparison
- `.siproject` project save/load
- Excel, PNG, CSV, and Mapping CSV export

> **Important Notice**  
> SParamView is an **engineering reference tool** that calculates and visualizes the supplied Touchstone data. Results can differ from the actual system or measurement because of input-data quality, frequency span, port/channel mapping, interpolation or approximation, termination conditions, and other analysis settings. Independently verify important engineering decisions against the original data and trusted measurement or analysis results.

In particular, **Quick TDR is an estimate derived from a finite frequency band** and is not a substitute for an instrument TDR measurement.

### 1.1 Changes relevant to v1.2.1 users

| Area | User-visible behavior |
|---|---|
| Input validation | Invalid numbers, ranges, and ambiguous input conditions are rejected safely. Review the input and settings when an error appears. |
| Initial TDR view | Typical zero-based results initially show approximately −0.1 to 1.5 ns. Use **Fit All** to inspect the full computed interval. |
| Selected TDR interval | The uniform FFT path no longer marks an otherwise valid selected interval `UNSUITABLE` solely because an impedance singularity exists outside it. |
| Worst Margin ranking | Within each analysis metric, results with defined margins appear first; missing-margin results form a separate group. |

The v1.2.0 source partitioning is an internal code-structure change and requires no extra user configuration. For the intervening changes, see [v1.1.6](Release_1.1.6.md), [v1.1.7](Release_1.1.7.md), [v1.2.0](Release_1.2.0.md), and [v1.2.1](Release_1.2.1.md).

---

## 2. Installation and First Launch

### 2.1 Choose the correct release package

Download the package that matches your system from GitHub Releases.

| System | Recommended package |
|---|---|
| Windows x64 | `SParamView_Windows_x64_v1.2.1.zip` |
| Windows ARM64 | `SParamView_Windows_ARM64_v1.2.1.zip` |
| macOS Apple Silicon | `SParamView_macOS_AppleSilicon_arm64_v1.2.1.dmg` or `.zip` |

SHA-256 files are also provided with the release. To verify download integrity, run `Get-FileHash <filename> -Algorithm SHA256` in Windows PowerShell or `shasum -a 256 <filename>` in macOS Terminal, then compare the result with the corresponding `.sha256` file.

### 2.2 Windows

Extract the ZIP package and launch SParamView. Windows reputation or security warnings may appear. Confirm that the file came from the official GitHub release, verify the SHA-256 value, and follow your organization’s security policy before running it.

### 2.3 macOS Apple Silicon

The current public macOS package is **ad-hoc signed** and is not Apple-notarized. Gatekeeper may therefore display a warning. Verify that the package came from the official GitHub release and confirm its SHA-256 value. If you trust the package, macOS may allow it through **System Settings > Privacy & Security** according to the system’s security policy.

> Whether to override an operating-system warning is a user or organization security decision.

---

## 3. Quick Start

For a first analysis, follow this sequence:

1. Open files with **+ Open Touchstone**.
2. Review Near/Far and P/N assignments and confirm **Channel Mapping**.
3. Select the Revision and Channels.
4. Choose the required metrics and press **QUICK ANALYSIS**.
5. Review Plot and Results.
6. Adjust Limits or analysis ranges if needed, then rerun analysis.
7. Use **Save Project** or export XLSX / PNG / CSV.

The status guidance at the bottom of the main window follows the same basic sequence: `Open file → Confirm mapping → Select channels → QUICK ANALYSIS`.

---

## 4. Main Window

> **Automated validation screenshot** — The image below was generated automatically by SParamView v1.2.1 during native Windows ARM64 validation using the synthetic sample project. Layout can vary slightly with OS, DPI, and window size.
> These screenshots were generated during public v1.2.1 release validation. See section 10 for TDR display and evaluation behavior. [Image provenance](images/README.md).

![SParamView v1.2.1 main window](images/01_Main_Window.png)

### 4.1 Header shortcuts

SParamView v1.2.1 provides these header buttons:

- **+ Open Touchstone**: open one or more Touchstone files
- **Open Project**: open an SParamView `.siproject`
- **Save Project**: save the current project
- **Open Sample**: load the included sample project
- **Export XLSX**: export completed analysis results to an Excel workbook

### 4.2 Channel area

- **REVISION / CHANNEL**: select the active revision
- **All groups**: filter by channel group
- **Search Channels...**: search channel names
- **Select All / Clear Selection**: select or clear all visible channels
- **Channel Mapping...**: review or edit physical-port-to-logical-channel mapping
- **Quick Analysis**: select RL / IL / NEXT / FEXT / TDR
- **QUICK ANALYSIS**: run the selected metrics

### 4.3 Analysis and view controls

- **Split / Plot / Results**: change the workspace layout
- **Advanced**: show advanced analysis settings
- **Freq**: analysis frequency range in GHz
- **Near → Far / Far → Near / Both**: choose the displayed direction
- **Worst / Top 3 / Top 5 / Off**: automatic marker count
- **Channels / Settings / Full Screen**: show/hide panels and full-screen mode

### 4.4 Plot and Results

The Plot area displays the current analysis traces. The Results area lists channel-level worst values, locations, margins, directional deltas, status, aggressor, quality information, and units/terminations.

---

## 5. Opening Touchstone Files

Use **+ Open Touchstone** or **File > Open Touchstone...**. SParamView accepts selectable `*.s*p` and `*.ts` files and can open multiple files as revisions for comparison.

After opening a file, **always review Channel Mapping**. Successful file parsing does not guarantee that Near/Far or P/N relationships can be determined unambiguously from port labels.

SParamView reads the original Touchstone data locally and does not modify the source file.

---

## 6. Channel Mapping

The following dialog was generated automatically during a validation case where mapping could not be confirmed from the input alone. In this situation, verify the physical port relationships and confirm them manually or with a Mapping CSV.

![SParamView Channel Mapping dialog](images/02_Channel_Mapping.png)

Channel Mapping is one of the most important steps for analysis accuracy. A mathematically correct calculation can still produce the wrong engineering result if the physical ports are mapped incorrectly.

### 6.1 Basic rules

- Physical port numbering starts at **1**.
- For a single-ended channel, leave `Near−` and `Far−` blank.
- A differential channel uses a Near P/N pair.
- If the Far differential endpoint is used, both Far P and Far N must be specified.
- A channel with the Far endpoint blank can support reflection/TDR-oriented workflows.
- Always verify Near/Far and P/N against the actual physical connections.

### 6.2 Mapping columns

| Column | Meaning |
|---|---|
| Channel | Logical channel name |
| Near+ / Near− | Near-end P/N ports |
| Far+ / Far− | Far-end P/N ports |
| Group | Group used by automatic NEXT/FEXT aggressor selection |
| Alias | User alias |
| Mapping ID | Identifier used to match logical channels across revisions |

### 6.3 Editing tools

The mapping dialog supports adding/deleting rows, suggestions from port labels, a single-ended half-split suggestion, manual differential pairing, P/N swapping, and Near/Far swapping. Mapping can also be reused with **Channel Mapping > Import Mapping CSV... / Export Mapping CSV...**.

If automatic mapping is uncertain, SParamView does not silently confirm it. User confirmation is required. Changing and saving mapping invalidates existing analysis results for that revision, so the analysis must be rerun.

### 6.4 100 Ω differential vs. two 50 Ω-to-ground loads

These are different termination configurations:

- **Differential P–N 100 Ω**: 100 Ω connected between P and N
- **P–GND 50 Ω + N–GND 50 Ω**: P and N are each connected to ground through 50 Ω

SParamView keeps these conditions separate in its TDR termination options.

---

## 7. Running Analysis

### 7.1 Quick Analysis

Select the required items under **Quick Analysis**, then press **QUICK ANALYSIS**. Available metrics are:

- RL
- IL
- NEXT
- FEXT
- TDR

At least one metric and one channel must be selected, and mapping must be confirmed for all loaded revisions.

### 7.2 Individual metric analysis

The **RL / IL / NEXT / FEXT / TDR** buttons in the Settings area show the existing result when it is already valid for the current analysis context. Otherwise, SParamView recalculates that metric.

### 7.3 Direction

The direction control cycles through:

1. **Near → Far**
2. **Far → Near**
3. **Both**

For RL and IL, when both directional results are available, the Results table can show `Max ΔDir [dB]` and `Δ @ GHz`.

---

## 8. RL / IL / NEXT / FEXT Interpretation

### 8.1 Return Loss

SParamView uses RL together with **S11** for single-ended data and **Sdd11** for differential data where applicable. The current limit relation is `≤`. For example, with a `-10 dB` limit, values at or below `-10 dB` satisfy the limit.

The automatically generated validation plot below shows an **Sdd11 Return Loss** example using synthetic differential sample data. The red dashed line is the enabled limit, and the plot shows worst points together with OK/NG status.

![Automated Sdd11 Return Loss validation plot](images/03_RL_Analysis_Graph.png)

### 8.2 Insertion Loss

IL corresponds to **S21** for single-ended data and **Sdd21** for differential data where applicable. The current limit relation is `≥`. For example, with a `-3 dB` limit, values at or above `-3 dB` satisfy the limit.

### 8.3 NEXT

NEXT represents coupling from a near-end aggressor to a near-end victim. Its limit relation is `≤`.

### 8.4 FEXT

FEXT represents coupling from a near-end aggressor to a far-end victim. Its limit relation is `≤`.

By default, NEXT/FEXT considers other channels in the **same Group** as aggressor candidates. A specific aggressor can be selected in **Advanced > Aggressor**.

---

## 9. Limits and OK / NG / N/A

Pass/fail evaluation is meaningful only when the corresponding user-defined limit is enabled.

| Analysis | Evaluation relation |
|---|---|
| RL | value ≤ Limit |
| IL | value ≥ Limit |
| NEXT | value ≤ Limit |
| FEXT | value ≤ Limit |
| TDR | Target Ω ± Tolerance % |

Result states are:

- **OK**: the enabled limit is satisfied
- **NG**: a limit violation is detected
- **N/A**: no valid pass/fail determination is available for that condition

> **N/A is not PASS.** It can appear when a limit is disabled, when a point lies outside a defined frequency-limit range, or when the required evaluation conditions are not available. If the input or computed result does not cover a requested frequency/time interval, a passing evaluated portion does not establish OK for the whole request. An observed violation keeps the verdict NG; incomplete requested coverage without an observed violation produces N/A. Read the Results notes/tooltips.

### 9.1 Frequency-dependent limits

Use **Limit > Edit Frequency Limits...** or **Advanced > Frequency Limit...** to enter `GHz, dB` points, for example:

```text
1, -10
3, -12
6, -15
```

The defined limit is not extrapolated beyond its specified frequency range; values outside that range are treated as N/A for that limit. Clear the text to return to a constant limit.

Limit settings can be saved and loaded as JSON presets.

---

## 10. Quick TDR

### 10.1 Important limitation

Quick TDR transforms finite-band Touchstone information into a time-domain engineering estimate. It is affected by factors including:

- available frequency span and frequency spacing
- availability and treatment of low-frequency/DC information
- interpolation, approximation, and windowing
- channel mapping
- reference impedance
- termination at the opposite end

Do not interpret Quick TDR as equivalent to an instrument TDR measurement.

### 10.2 Target and display scale

With `Auto Z₀`, the UI uses separate TDR views for single-ended and differential channels, nominally **50 Ω** for single-ended and **100 Ω** for differential. A target impedance can also be entered manually.

### 10.3 TDR limit

When the TDR limit checkbox is enabled, evaluation uses **Target ± Tolerance %**. For example, 100 Ω with ±10% produces an allowed range of 90–110 Ω.

### 10.4 Termination options

Use the TDR gear (⚙) button to select one or more conditions for overlay:

- **Reference**: matched to the source reference impedance
- **Resistive**: SE 50 Ω / Differential P–N 100 Ω (floating)
- **Grounded**: SE 50 Ω / Differential P–GND 50 Ω + N–GND 50 Ω
- **Open**: SE open / Differential P and N both open
- **Short**: SE–GND short / Differential P–N short (floating)

In Forward analysis the selected termination is applied at the Far end; in Reverse analysis it is applied at the Near end. Other physical ports are matched to their source reference impedances. The two single-ended 50 Ω conditions are electrically equivalent in this model and are calculated once.

Enable **Show Reflection Coefficient ρ** when reflection-coefficient display is more useful, especially for open/short comparisons. Impedance-tolerance limits do not apply to ρ.

### 10.5 Initial viewport versus analysis interval

Typical zero-based TDR results initially use an X-axis of approximately **−0.1 to 1.5 ns**. The margin before zero is display space; it does not create negative-time samples. For results that start materially away from zero, the initial view starts approximately 0.1 ns before the first computed sample and spans approximately 1.6 ns.

**The initial viewport, Zoom/Pan, and Axis Limits change the display.** To change the interval used for TDR calculation and evaluation, edit Start/Stop under **Advanced > TDR time [ns]** and rerun analysis. Stop set to **Auto** uses the available computed interval. The attainable time range remains limited by the input frequency spacing and other transform conditions.

- **Fit All**, or **Home** with the Plot focused: display the full interval of the current computed result.
- **Previous View** or **Backspace**: restore the previous viewport.
- Pressing the TDR metric button again or switching between Single-ended / Differential TDR restores the initial view.
- A manual Zoom/Pan viewport is preserved during repaint and detail refresh of the same data view.

Automatic Y scaling can prevent large termination-related impedance values from dominating the initial view. **Values outside the automatic viewport are not deleted from the calculation or evaluation.** Review Fit All and Results together when inspecting the complete computed result.

### 10.6 Quality and selected-interval suitability

`Quality` describes Quick TDR calculation conditions and is separate from the `Result` verdict OK / NG / N/A. **GOOD does not mean OK or guarantee measurement accuracy.**

| Quality | Interpretation |
|---|---|
| GOOD | The calculation path did not classify the result as limited or unsuitable. Input data and mapping still require review. |
| LIMITED | Approximation, termination, or other conditions limit interpretation. Read the Results notes/tooltips. |
| UNSUITABLE | The current input/transform conditions are unsuitable for impedance evaluation. A waveform may be unavailable or contain invalid samples. |

In v1.2.1, the uniform FFT path checks impedance singularities **inside the time interval selected in Advanced**. A singularity outside that interval alone does not make a valid selected interval unsuitable. A singularity inside the interval can still produce `UNSUITABLE` for Reference termination and the existing `LIMITED` classification for other termination views. Other input, transform, and range checks remain applicable.

For example, a synthetic regression fixture has approximately 50 Ω from 0 to 1 ns and a singularity from 5 to 6 ns. **Setting TDR time to 0–1 ns and rerunning analysis** avoids inheriting that later singularity's unsuitable classification. Merely zooming the plot to 0–1 ns does not change the analysis interval. This is a synthetic test example, not a guarantee of GOOD or OK for every real input.

---

## 11. Plot Controls

| Control | Function |
|---|---|
| Mouse Wheel | X-axis zoom |
| Ctrl + Wheel | Y-axis zoom |
| Drag | Pan |
| Shift + Drag | Box zoom |
| `+` / `−` | Zoom in/out |
| Home | Fit All |
| Backspace | Previous View |
| Axis Limits | Set X/Y ranges directly |
| Fit All | Return to the full data range |

Heatmap and Margin views are available for non-TDR plots. Moving the pointer over the plot shows the current frequency/time and displayed trace value.

---

## 12. Reading the Results Table

Important columns include:

- **Channel**: logical channel
- **Revision**: source revision
- **Analysis**: RL / IL / NEXT / FEXT / TDR
- **Parameter**: displayed S-parameter such as S11, S21, or Sdd11
- **Worst**: worst value in the analysis range
- **GHz / ns**: location of the worst value
- **Margin**: margin to the active limit
- **Max ΔDir [dB] / Δ @ GHz**: maximum RL/IL directional difference
- **Result**: OK / NG / N/A
- **Aggressor**: NEXT/FEXT aggressor
- **Quality**: calculation/preview quality information
- **Termination / Unit**: TDR termination and Ω/ρ, or dB

The summary area shows overall OK / NG / N/A counts. Rows can be ranked by **Worst Margin / Worst Value / Channel / Result**.

### 12.1 Interpreting Worst Margin ranking

Within each analysis metric, **defined margins sort first, from smallest to largest**. Missing-margin results form a separate group after them and use Worst Value severity ordering. Equal defined margins also use Worst Value severity. If both margin and severity are equivalent, the previous order is retained; this is not a guarantee of alphabetical channel order.

| Example within one metric | Margin | Worst Margin position |
|---|---|---|
| Channel C | −1 dB | 1st |
| Channel A | 0 dB | 2nd |
| Channel B | Missing | 3rd |

A missing margin does not mean ample margin. Check for a disabled limit or unavailable evaluation conditions. **Worst Value** prioritizes larger RL/NEXT/FEXT values, smaller IL values, and larger absolute deviations from Target Ω for TDR.

---

## 13. Comparing Revisions

When multiple Touchstone files are loaded as revisions, matching logical channels can be compared.

- At least two revisions are required.
- Mapping must be confirmed for every revision.
- RL/IL/NEXT/FEXT can be compared using dB differences.
- TDR comparison is performed using plot overlays rather than a dB-delta comparison table.

For a meaningful comparison, verify that the same physical signal maps to the same logical channel identity across revisions.

---

## 14. Saving and Restoring Projects

### 14.1 Save

Use the header **Save Project** button or **File > Save Project**. Use **File > Save Project As...** to choose a new file name.

A `.siproject` stores analysis settings, revision information, channel mapping, selected channels, selected UI state, and source Touchstone path/hash information.

> **Important:** A `.siproject` does not fully embed the original Touchstone data. When moving a project to another computer, keep the related Touchstone files with it.

### 14.2 Open

Use the header **Open Project** button or **File > Open Project...**. SParamView first tries the stored relative source path, then the stored absolute path. If the source file still cannot be found, it asks the user to locate the Touchstone file.

If the source Touchstone SHA-256 differs from the value stored in the project, mapping confirmation for that revision can be cleared and should be reviewed again.

---

## 15. Export

### 15.1 Excel Workbook

Use **Export XLSX** or **File > Export Excel Workbook...**. Available content options are:

- Summary
- Detailed results
- Graphs
- Raw trace data (large)

Raw trace data can significantly increase workbook size.

### 15.2 Plot PNG

Use **File > Export Plot as PNG...** to save the current plot.

### 15.3 Raw CSV

Select **exactly one row** in Results, then choose **File > Export Selected Trace as CSV...** to export the raw trace data for that result.

### 15.4 Mapping CSV

Use the Channel Mapping menu to import or export Mapping CSV files for reuse with compatible port structures.

---

## 16. Advanced Settings

Expand **Advanced** to access:

- **Marker [GHz]**: comma-separated manual frequency markers
- **Performance**: Low / Normal / Maximum
- **Aggressor**: NEXT/FEXT aggressor selection; default is `AUTO (same group)`
- **TDR time [ns]**: TDR time range
- **Peak prominence**: sensitivity-related setting for automatic peak detection
- **Frequency Limit...**: frequency-dependent limit for the current dB metric

After changing analysis settings, **rerun analysis** for the changes to affect calculated results. Export uses the settings from the last completed analysis.

---

## 17. Useful Menus

### File
Open Touchstone/Project files, save projects, and export XLSX/PNG/CSV.

### Channel Mapping
Edit/confirm mappings and import/export Mapping CSV files.

### View
Provides Split View, Maximize Plot, Maximize Results, Channel List, Analysis Settings, Full Screen, and Reset Layout. `Ctrl+1 / Ctrl+2 / Ctrl+3` switch workspace modes, and `F11` toggles full screen.

### Limit
Save/load limit presets and edit frequency-dependent limits.

### Tools
- **Open Sample Project**
- **Verify Source SHA-256**: verify that the current Touchstone source still matches the source hash used for analysis

---

## 18. Troubleshooting

| Symptom | Check |
|---|---|
| Touchstone opens but analysis cannot run | Confirm Channel Mapping |
| Mapping is not automatically confirmed | Port labels may be ambiguous; use manual mapping or Mapping CSV |
| QUICK ANALYSIS does not run | Select at least one channel and one analysis metric |
| No NEXT/FEXT result | Check for a valid same-group aggressor and valid Near/Far mapping |
| Result is N/A | Check whether the limit is enabled, whether the point is inside the frequency-limit range, and whether evaluation conditions exist |
| Settings changed but results did not | Rerun analysis after changing settings |
| Later TDR times are absent from the initial view | Use **Fit All**; to change the analysis interval, edit **TDR time [ns]** and rerun |
| Apparently normal TDR interval is N/A | Check Quality, Limit, selected interval, and attainable range; zooming alone does not change analysis coverage |
| Missing-margin rows appear later | Expected v1.2.1 Worst Margin ordering; missing margin is separate from an OK verdict |
| TDR differs greatly from expectation | Review frequency span/spacing, mapping, target Z₀, termination, and source-data quality |
| Project cannot find its source file | Relocate the Touchstone file; reconfirm mapping if the file changed |
| CSV export fails | Select exactly one Results row |
| XLSX export fails | Complete an analysis first |
| macOS launch warning appears | Verify official release/SHA-256 and follow macOS Privacy & Security policy |

For a reproducible issue report, record the SParamView version, OS/architecture, Touchstone port count, exact reproduction steps, and any error message.

---

## 19. Sample Data

The repository `examples/` directory contains synthetic demonstration fixtures for UI and regression testing. They are not real product measurement data. Use **Open Sample** or **Tools > Open Sample Project** to explore the basic workflow.

---

## 20. Data and Privacy

SParamView performs analysis locally and treats the original Touchstone source as read-only. Project and export files are written to user-selected local paths.

---

## 21. Support and License

SParamView source is distributed under the MIT License. Third-party components and fonts retain their respective licenses. See `LICENSE`, `COPYRIGHT.txt`, and `third_party/` in the repository for details.

Contact: **sparamview@gmail.com**

---

## 22. Documentation verification sources

The revised instructions were checked against the [v1.2.1 release notes](Release_1.2.1.md) and the following release sources:

- [Initial TDR axes and automatic scaling](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/src/plot.cpp)
- [Plot interaction and viewport restoration](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/src/navigation.cpp)
- [Selected-interval TDR suitability](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/src/tdr.cpp)
- [Result ranking](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/src/jobs.cpp)
- [Requested coverage and OK/NG/N/A evaluation](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/src/analysis.cpp)
- [Ranking and TDR interval regression tests](https://github.com/july-rust/SParamView-Public/blob/v1.2.1/tests/v121_regression_tests.cpp)

---

### Document Version

This guide was revised against the UI and source of the **SParamView v1.2.1 public release** on 2026-10-04. Screenshots have been refreshed from the public v1.2.1 automated validation results. Menus, functions, or analysis settings may change in later versions.
