# 012_06 — Generic Integrated Crosstalk Noise (ICN) numerical/Qt contract

**Baseline:** official v1.3.5 remains unchanged.  
**Target:** `integration/v1.4.0` (unreleased development).  
**Work:** 012_06; PR [#42](https://github.com/july-rust/SParamView-Public/pull/42).  
**Qualification:** CI and protected squash merge must be verified; no automatic claim of COMPLETE.  
**Conformance:** **NO IEEE 802.3 / SNIA SFF-TA-1002 / COM / MIPI conformance or PASS/FAIL**.

## Inputs and physical source checks

- `SourceSnapshot::capture` recomputes SHA-256 on the original Touchstone file in the worker, verifies confirmed physical port mapping and generation.
- User selects a victim channel and **1–32 individual NEXT/FEXT paths** from a list of aggressor channels; physically identical victim/aggressor or incompatible differential modes/groups fail closed.
- Each path uses original `extract(snapshot,victim,Metric::NEXT/FEXT,reverse,aggressor)` with complete complex S-parameters. Direction is source-qualified, not inferred from graph labels.
- Same qualified Touchstone frequency grid and equal stimulus/response reference impedances are required; otherwise analysis fails rather than treating mismatched impedance as normalized.
- Different source files/unknown reference planes are not auto-combined. At most 2,097,152 path-bin products and 262,144 source frequency points are accepted.

## Explicit exploratory TX/RX assumptions

Generic illustrative IID NRZ bit stream, equiprobable symbols of ±Vpp/2 across the selected **single-ended or differential** mode, fixed independent random aggressors, ideal rectangular NRZ pulses, and optional Gaussian transition smoothing.

- UI: symbol rate in GBd, TX peak-to-peak voltage in mVpp, **10–90%** Gaussian rise time in ps, RX one-pole -3 dB bandwidth in GHz.
- With T = 1/baud, sigma_v = Vpp/2, x=π f T, Gaussian sigma_t = t_10–90/2.563103...:
  - `S_TX,1-sided(f) = 2 sigma_v² T [sin(x)/x]² exp[-(2π f sigma_t)²]` in V²/Hz.
  - `|H_RX(f)|² = 1 / [1 + (f/f_3dB)²]`.
- Each path's voltage-noise density: `D_k(f)=S_TX(f) |H_RX(f)|² |S_path,k(f)|²` in V²/Hz.
- Integrate *only measured frequencies in the requested intersection* using trapezoidal quadrature, with complex linear interpolation for exact band endpoints.
- Individual RMS: `σ_k = sqrt(∫D_k df)`; NEXT/FEXT squared sums by group; Total `σ = sqrt(Σ σ_k²)`.
- Contribution is `100 σ_k²/σ_total²` percent.
- One-sided PSD integrates **positive frequencies only**. No duplicate factor of 2, no amplitude/power-sum confusion.
- A path whose complex transfer is exactly zero contributes 0 V noise; -300 dB is only the plotted zero floor for a finite GUI line, **not** a numeric noise floor.

Only TX/RX assumptions above support the displayed mV RMS figure. In particular, **missing/zero Vpp, symbol rate or RX bandwidth is rejected**, not silently set to nominal by the numerical engine. GUI shows explicit editable initial values; these values are *illustrative defaults, not source-derived actual device settings*.

## Band-limited quality

- If the requested band extends beyond measured frequencies or the integration begins above DC, results are **WARNING / BAND LIMITED**; out-of-band content is **never fabricated**.
- A band with no intersection, corrupted NaN/Inf transfer, invalid port mapping or mismatched frequency grids is rejected.
- Results should never be interpreted as full-band system noise, BER, PHY margin or standard ICN. Inter-aggressor correlation/covariance, simultaneous switching, modulated PAM4/PAMn PSD, IEEE-specific TX/RX filters and COM package/equalizer models are outside 012_06.
- A reversal of a complex path sign does not alter RMS under this **independent power sum** assumption. That does not imply cancellation of physically coherent correlated interferers.

## Independent GUI

Select `Analysis → ICN` (top-level nonmodal window). Choose validated source, victim and NEXT/FEXT paths; configure frequency range, baud, mVpp, 10–90 rise time, RX bandwidth; Run or Cancel.

The independent Qt worker captures the immutable cache and SHA-validated snapshot without accessing GUI objects. Channel/revision/port mapping/settings changes cancel work or discard stale completion. A closed ICN window must not affect ILD, Eye/COM placeholders, or the main window.

Displays:
1. NEXT/FEXT summed transfer power in dB (zero plotted at -300 dB).
2. NEXT/FEXT weighted PSD in mV²/GHz.
3. NEXT/FEXT cumulative integrated RMS in mV, plus a textual total RMS and path count.

CSV: source SHA, revision, mapping identity, algorithm/settings, effective frequency bounds, path ID/type/parameter/RMS/power percentage and per-bin power/PSD/cumulative RMS. Metadata strings use quoting/formula-injection defense. PNG contains the real rendered ICN plot; both use atomic `QSaveFile` and reject a STALE result.

## Qualification

- Qt-free analytic oracle (trapezoidal NRZ integration independently coded), zero, multi-path RSS, 2× Vpp scaling, sign reversal, nonuniform bins, no-DC warning, missing Vpp, duplicate path, NaN, cancellation.
- Actual s4p fixture → cache SHA → two physically mapped channels → two NEXT/FEXT paths → UI worker → actual numerical plot → CSV/PNG readback → stale export rejection.
- Legacy ILD, RL/IL/NEXT/FEXT/TDR, Touchstone/Mapping/project and three-platform CI must remain passing.
- Automated offscreen/CI does **not** substitute for physical interactive Windows/macOS platform qualification at 012_12.

## Sources

- [Controlling 012_01–012_14 plan](Development_Plan_012_v1.4.0.md)
- [Common Channel Analysis Core 012_03](Channel_Analysis_Core_012_03.md)
- [ILD precedent 012_05](Channel_Analysis_ILD_012_05.md)
- [IEEE 802.3ck ICN discussions](https://www.ieee802.org/3/ck/public/)
- [SNIA SFF-TA-1002](https://members.snia.org/document/dl/27231) — ccICN is a **different reference model**.
