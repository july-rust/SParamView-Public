# SParamView v1.4.3

Maintenance candidate following [v1.4.2](Release_1.4.2.md) — 016 Unified Auto-to-Manual Configuration & Eye Diagram Numerical Requalification.

## Changes

- Successful Auto Analysis/Auto Preview fills Manual controls with resolved settings. Unedited frequency and ICN numeric controls retain calculation precision when another field is changed. Review assumptions before applying known conditions. PHY Profile is removed; Eye overlay includes TX incident voltage as a visual reference. COM remains deferred.
- Preserve mapped net identities, direction and source provenance; discard superseded work.
- Retain deterministic periodic NRZ frame, existing DC/AC checks, Multi View and engineering-reference-only scope. No BER, PHY compliance or actual board correlation is claimed.

## Qualification

Earlier candidate `60ca2d45fd583dfb2ff1cbc2becd973340a007bf` passed [three-platform CI and native ARM64 package gate](https://github.com/july-rust/SParamView-Public/actions/runs/46120571235) and [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46120571246). GUI regression includes two nets with duplicate labels in a synthetic four-port file and a 2:1 independent gain ratio. This is not final commit evidence or actual product S36P measurement evidence. At that earlier candidate stage final CI, real S36P checks and fresh screenshots were pending; see newer evidence below.

## Downloads

[Release and checksums](https://github.com/july-rust/SParamView-Public/releases/tag/v1.4.3) when published by the validated release workflow.

### Latest verified candidate evidence

Commit `2f254f905326c3a20e110804c55e49f4067864d0` passed all nine non-publication jobs in [three-platform CI](https://github.com/july-rust/SParamView-Public/actions/runs/46134402405), including native Windows ARM64 packaged-binary qualification, and [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46134402409). This includes resolved-input CSV/PNG identity parity and fractional-rate preservation. [Native evidence archive](https://github.com/july-rust/SParamView-Public/actions/runs/46134402405/artifacts/15259787666) was downloaded and inspected. The `workspace/language_en_2.png` capture shows the actual v1.4.3 Eye window without PHY Profile and with readable labels. The separate `analysis-screenshots/` captures show missing glyphs on the offscreen runner; do not use them as manual illustrations until corrected. Original product S36P files remain unavailable. No real board correlation or final release is claimed.

## Visually inspected v1.4.3 candidate captures

Commit `9ba7146c22d0c3adbfb61acee92d8285b72c821d` passed all nine non-publication jobs ([CI](https://github.com/july-rust/SParamView-Public/actions/runs/46136603711), [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46136603714)). The [native ARM64 evidence archive](https://github.com/july-rust/SParamView-Public/actions/runs/46136603711/artifacts/15259804829) was downloaded and opened: `analysis-screenshots/04_ILD.png`, `05_ICN.png` and `06_Eye_Multi_View.png` are readable 1024 x 768 actual Qt captures with bundled font; the Eye view uses a scrollable third plot and no PHY selector. `07_Eye_Multi_Plot.png` is 264 x 876 with readable glyphs but narrow/truncated titles, not a recommended manual illustration. The archive uses synthetic fixtures, not real S36P board measurements. The three verified current images are checked into the repository byte-for-byte (import commit `f5f2082fd43b6310a3ca026b15bfb10240815a20`); the remaining v1.4.2 images are explicitly historical. The narrow new combined PNG is not used as a guide illustration. Real S36P validation remains pending; no release publication claimed.

## Final-review overlay correction (017)

The prior candidate selected the first 64 two-UI traces only; a long initial run could hide logic levels/transitions although height and width were calculated across the whole frame. The bounded overlay now selects across the entire periodic frame, preserving each present 00/01/10/11 class and both actual traces defining the reported maximum-height phase. Numerical FFT and metric formulas are unchanged. New independent regression cases cover a late transition, circular boundary, deterministic selection, and height-defining traces. Qualification of this correction requires its own exact-head CI; earlier captures and CI are historical candidate evidence. No measured BER or exhaustive physical board correlation is claimed.
