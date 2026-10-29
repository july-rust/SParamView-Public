# SParamView v1.3.0 — Release notes

**State:** **Official stable release**, published **2026-10-29 11:43 KST**. [Release assets](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.0) · [successful GitHub Actions](https://github.com/july-rust/SParamView-Public/actions/runs/40807027224) · release commit `dd4354367c49dc25912ed83bbeb41b284e29f616`.

## Touchstone compatibility

- The S-parameter viewer accepts legacy Touchstone **1.0 / 1.1** (implicit version) and **2.0 / 2.1** (explicit `[Version]`) syntax. There is no approved Touchstone **1.9**.
- Existing Hz/kHz/MHz/GHz frequency units, RI/MA/DB encodings, full/lower/upper matrices, per-port references, conventional two-port ordering and declared mixed-mode data continue to be supported.
- Importer recognizes the valid exporter-specific comment `! PortN_COMPONENT_PIN::NET` in addition to `! Port N = NAME`. Physical port labels retain their component/pin and NET.
- Vendor `!.DiffChannels` is not a normative Touchstone keyword. Its metadata is accepted only after physical indexes, net names, P/N polarities and (for scoped labels) exact endpoint names agree.
- Binary cache revision increased from 6 to 7. Stale port labels from earlier parser generations cannot silently survive re-import.
- Y/Z/H/G Touchstone network parameter kinds require separate validated conversions; they are **explicitly unsupported by the current S-parameter-only analyzer**, not misread as S data.

## Windows package

The package root has one EXE: `SParamView.exe`. Qt DLLs and required runtime DLLs stay next to it. The bundled CLI moves to `tools/si_cli.exe`; test EXEs to `tests/si_*.exe`, with `tests/fonts/` for deterministic Qt tests. Root-level `Run_Windows_Verification.cmd`, CI native ARM64 execution, and release publication audit were updated to use the new locations.

## Regression / quality gates

- Synthetic 24-port / 40-port v1.x, v2.0 and v2.1 Touchstone input, correct name/port/net and P/N direction, cache reopen and compatibility with the legacy port syntax.
- Explicit source endpoint mismatches must not be accepted as trusted differential metadata.
- Core sanitizer/CTest, Windows x64, Windows ARM64, native Windows ARM64, Apple Silicon macOS and release SHA/provenance checks **passed** before publication.
- **Private uploaded design data were not committed to this public repository.** Vendor-syntax compatibility is tested using generated synthetic fixtures.

The tagged release and its assets are immutable. Subsequent repository documentation clarification does not replace previously published binaries or guide attachments.

Reference: IBIS Open Forum Touchstone v2.1, https://ibis.org/touchstone_ver2.1/touchstone_ver2_1.pdf .
