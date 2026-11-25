# 012_05 — ILD engine and GUI contract (v1.4.0 integration)

**012_44 amendment (qualification pending exact-head CI):** The locked requirements specify bandwidth-weighted least squares and band-integral RMS; the original 012_05 unweighted implementation below is historical. The 012_44 candidate uses normalized trapezoidal cell weights in QR, default Root + Linear, and exact integration of the squared piecewise-linear residual over the actual measured band. The three original models remain selectable. The CSV identifies the weighting, RMS definition, requested and actual ranges, and peak frequency. Zero residual chooses the first measured frequency. Generic ILD remains non-normative; no IEEE FOM or compliance claim. Independent weighted linear, nonuniform, exact-model, CSV, directional S2P/S4P/S6P and out-of-band oracles are included; actual execution evidence is required before completion. The source contract retains normalized f/F (equivalent model space to f/GHz, coefficients are explicitly normalization-dependent).

**Release baseline:** official v1.3.5 unchanged.  
**Development target:** integration/v1.4.0.  
**Numerical status:** **COMPLETE** for exploratory generic ILD scope. PR #40 squash-merged to `integration/v1.4.0` as `90a59ada15a9c17aa787ffb317fcafefb4285c75`; [CI 43766947020](https://github.com/july-rust/SParamView-Public/actions/runs/43766947020) passed Core 33/33 Release + 33/33 ASan/UBSan, Windows x64 52/52, macOS arm64 48/48 and native Windows ARM64 packaged gate. Real hardware interactive UI remains 012_12.  
**Standards status:** generic ILD only; **NOT** an IEEE-weighted FOM_ILD or MIPI PHY conformance statement.

## Calculation and sign convention

Use the selected confirmed channel and forward or reverse direction. Single-ended
transmission uses S21/S12; differential uses the properly mapped Sdd21/Sdd12.
The immutable source revision, physical mapping and actual original file SHA-256
are verified on the worker before extraction.

- Positive insertion loss in dB: **IL(f) = -20 log10(|S21(f)|)**.
- Fit model uses **x = f/F** where **F** is the maximum measured frequency in
  the selected fit window (Hz). The default generic model is
  **IL_fit(f) = a0 + a1 sqrt(x) + a2 x + a3 x²**.
- Other supported generic models: a0+a1 x; a0+a1 sqrt(x)+a2 x.
- **ILD(f) = IL(f) - IL_fit(f)** (signed, dB).
- Peak absolute ILD is max(|ILD|) and RMS is sqrt(mean(ILD²)) over **actual
  measured bins**. Nonuniform bins are not interpolated or frequency-weighted.
- Numerically solve the scaled least-squares system with twice-reorthogonalized
  QR, rejecting rank-deficient and non-finite cases; neither direct normal
  equation inversion nor frequency extrapolation is performed.
- Frequency range uses measured bins within [start, stop]. A requested stop
  above the source bandwidth does not invent samples; actual fit range is
  reported in CSV. At least one additional sample beyond the fit model's
  coefficient count is required.

**Interpretation:** Generic unweighted RMS and peak figures do not implement a
standard-specific weighted FOM; no PASS/FAIL threshold or universal IEEE or MIPI
limit is claimed.

## GUI and output

Open **Analysis → ILD** once the real engine is registered. The ILD window is
top-level/nonmodal; other unfinished ICN/Eye/COM modules remain disabled.
Choose revision, channel, direction, frequency range and fit model; press Run.
Results are computed outside the GUI thread. Cancellation, project/revision/
mapping changes and window destruction invalidate pending results, prohibit
STALE exports and prevent stale callbacks.

Plot 1: original positive IL and fitted IL. Plot 2: signed ILD and its zero
reference. GUI displays RMS and peak absolute deviation at its frequency.

CSV includes source SHA, revision, mapping identity, numerical settings, model,
frequency normalization, fit coefficients, actual range, RMS/peak and per-bin
frequency/IL/fit/residual. Quotes and spreadsheet formula protection are applied
to metadata. Numerical precision is 17 significant digits. PNG exports the
actual computed plot with a save-file commit.

## Tests and outstanding gates

- Qt-free analytical golden: exact polynomial channel, exact linear, deliberate
  sinusoidal ripple, subrange filtering, zero/NaN transfer, too few samples,
  reversed band, cancellation.
- Qt GUI: real cached synthetic s2p with verified hash and confirmed physical
  channel mapping, actual ILD execution, plot, export-button enablement, STALE
  invalidation, and inherited independent-window/cancellation tests.
- Existing core, RL/IL/NEXT/FEXT/TDR, CSV/project, cache lifetime and required
  three-platform **automated CI passed on the PR head**. Re-verify on the post-merge integration SHA separately.
- An independent implementation or analytical construction is used as the
  numerical oracle; no assertion of measured lab waveform or official PHY
  compliance.
- Physical interactive Windows/macOS GUI verification is a separate **012_12**
  release gate; automated offscreen tests do not substitute for it.

This page is an implementation/qualification record, **not a release note**.
