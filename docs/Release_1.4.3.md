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
