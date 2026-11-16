# SParamView v1.4.0 — 012_08 Eye GUI Integration Contract

**Branch:** `integration/v1.4.0` via [PR #47](https://github.com/july-rust/SParamView-Public/pull/47).
**Scope:** Native Qt6 independent simulated-NRZ Eye window, verified SHA-backed source, async numerical engine, plot/CSV/PNG. **Not official v1.4.0 release**; v1.3.5 remains unchanged.

## Execution and provenance

The existing Analysis menu enables **Eye Diagram** only when the real `EyeSourceProvider` is registered. The standalone non-modal window shares no result state with ILD/ICN. Reopening focuses the same window; cancellation and destruction prevent delayed callbacks. `SourceSnapshot::capture` rehashes the original Touchstone file, confirms the channel mapping, and `extract(..., Metric::IL, reverse)` obtains S21/S12 or Sdd21/Sdd12 with complex phase and reference impedances. The GUI never substitutes a magnitude-only S21. Results are discarded after source SHA, mapping, active revision, generation or UI changes, and cannot be exported while STALE.

The numerical core is `calculateEye()` from 012_07. The result is deterministic, simulated, periodic NRZ with ideal zero response above the selected measured analysis cap. It is **not** an oscilloscope eye, statistical BER, time-domain de-embedded measurement, eye mask, PHY voltage conformance, or actual receiver equalization. GUI quality is always **SIMULATED_ONLY / WARNING** if calculations succeed.

## Settings

| Group | User interface | Interpretation |
| --- | --- | --- |
| Profile | MIPI D-PHY v2.1 HS; DDR2/3/4/5; LVDS; SLVS; SLVS-EC; Custom | Named examples/identities, **not** electrical conformance |
| NRZ rate | Gbps | Coupled: reference frequency = rate / 2; changing either updates the other |
| Independent cap | GHz | Must be measured through this frequency, **including measured DC** |
| Signal mode | Explicit single-ended/differential | Must agree with confirmed physical channel and through parameter |
| Pattern | PRBS7, alternating, custom 0/1 bits | Custom bit sequence validated by 012_07 (16..N/samples-per-UI symbols) |
| Sampling | FFT length and samples/UI | Radix-2 bounded by the numerical contract |
| TX incident low/high | mV | Voltage *incident at the S-parameter reference plane*, not assumed physical driver VOD |
| Source/receiver termination | ohm | **Matched, equal-real-Zref**; arbitrary Thevenin / mismatch renormalization unsupported |
| RX threshold/min/max | mV | Decision level and declared linear input range; out-of-range result rejected |

MIPI D-PHY HS example provides **4.5 Gbps/lane**, **2.25 GHz reference**, **10 GHz independent analysis cap**, and differential mode. It **does not** supply fabricated TX swings, terminations, thresholds or common-mode values; the user must enter these explicitly. Selecting a new profile clears electrical fields. Non-MIPI profiles require explicit rate, reference, cap and mode.

The **Analysis cap is distinct from the 2.25 GHz reference**, not the maximum Touchstone frequency by default. Missing/contradictory values or missing DC/10 GHz coverage block the computation.

## Visualization and exports

The Eye Plot widget exposes 2-UI received-voltage overlays, receiver decision threshold, sampling-phase guides and high/low boundaries. Alternate views show TX/RX time waveforms or impulse/unit-pulse response. The plot supports wheel zoom, drag pan, hover cursor and double-click reset. Metric labels include eye height (mV), eye width (ps/UI), optimal sampled phase (UI), delay (ps), received high/low extrema, bit rate, reference, and analysis cap.

The CSV records SHA, revision, physical mapping, algorithm/version, complete numerical settings including electrical values, simulated quality, computed metrics, time-domain TX/RX/impulse/pulse points and overlay points. Dynamic text cells are spreadsheet-formula protected and stored using `QSaveFile`. The PNG captures the rendered plot and prominent **Simulated Channel Eye — Not Measured / Not PHY Compliance** label; both exports are disabled for invalid/cancelled/stale results.

## Acceptance and restrictions

- Required PR CI: Linux Core + ASan/UBSan; Windows x64; Windows ARM64 build and native packaged-binary gate; macOS Apple Silicon arm64; consistency checks.
- Qt offscreen regression: source-based launch, voltage missing/not-evaluated, mismatched reference termination fail-closed, reference/rate coupling, graph/metrics, real CSV/PNG readback, SHA mutation → STALE.
- The actual user-operated physical Windows/macOS GUI, additional native visual quality and performance checks remain the separate **012_12** final release gate.
- No physical MIPI/DDR/LVDS/SLVS eye-mask, BER or COM qualification occurs in 012_08; do not claim standards PASS.
- `012_08 COMPLETE` only after the final PR head CI and protected squash merge **plus post-merge integration qualification**. Until then, mark as **IN PROGRESS**.

## Sources

- [012_07 numerical contract](Channel_Analysis_Eye_012_07.md)
- [012 numbered plan](Development_Plan_012_v1.4.0.md)
- [012_04 GUI worker contract](Channel_Analysis_GUI_012_04.md)
- [Public Qt6 QPainter documentation](https://doc.qt.io/qt-6/qpainter.html)
