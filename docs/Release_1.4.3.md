# SParamView v1.4.3

Maintenance candidate following [v1.4.2](Release_1.4.2.md) — 016 Unified Auto-to-Manual Configuration & Eye Diagram Numerical Requalification.

## Changes

- Successful Auto Analysis/Auto Preview fills Manual controls with resolved settings. Unedited frequency and ICN numeric controls retain calculation precision when another field is changed. Review assumptions before applying known conditions. PHY Profile is removed; Eye overlay includes TX incident voltage as a visual reference. COM remains deferred.
- Preserve mapped net identities, direction and source provenance; discard superseded work.
- Retain deterministic periodic NRZ frame, existing DC/AC checks, Multi View and engineering-reference-only scope. No BER, PHY compliance or actual board correlation is claimed.

## Qualification

Earlier candidate `60ca2d45fd583dfb2ff1cbc2becd973340a007bf` passed [three-platform CI and native ARM64 package gate](https://github.com/july-rust/SParamView-Public/actions/runs/46120571235) and [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46120571246). GUI regression includes two nets with duplicate labels in a synthetic four-port file and a 2:1 independent gain ratio. This is not final commit evidence or actual product S36P measurement evidence. Final CI, real S36P checks and fresh screenshots remain pending at candidate preparation.

## Downloads

[Release and checksums](https://github.com/july-rust/SParamView-Public/releases/tag/v1.4.3) when published by the validated release workflow.

### Latest verified candidate evidence

Commit `2f254f905326c3a20e110804c55e49f4067864d0` passed all nine non-publication jobs in [three-platform CI](https://github.com/july-rust/SParamView-Public/actions/runs/46134402405), including native Windows ARM64 packaged-binary qualification, and [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46134402409). This includes resolved-input CSV/PNG identity parity and fractional-rate preservation. [Native evidence archive](https://github.com/july-rust/SParamView-Public/actions/runs/46134402405/artifacts/15259787666) was downloaded and inspected. The `workspace/language_en_2.png` capture shows the actual v1.4.3 Eye window without PHY Profile and with readable labels. The separate `analysis-screenshots/` captures show missing glyphs on the offscreen runner; do not use them as manual illustrations until corrected. Original product S36P files remain unavailable. No real board correlation or final release is claimed.
