# SParamView v1.3.3 — Touchstone parser and channel mapping hardening

**Release targets:** Windows x64, Windows ARM64, macOS Apple Silicon arm64.

**Publication rule:** The official release, platform packages, checksums and validation evidence are published only after a complete successful three-platform GitHub Actions release job on `main`. An existing release tag or published package is never replaced.

## Scope

- **PowerSI `.DiffChannels`**: Require complete, zero-based unsigned decimal port index tokens; reject fractional, signed, overflowing or out-of-range values. Convert to `int` only after range validation. Retain diagnostics and conservative fallback for unconfirmed hints.
- **Physical endpoint evidence**: Parse the optional `PortN_` prefix without a recursive regular expression and confirm vendor-scoped physical endpoint names exactly as before.
- **Mixed-mode order**: Parse `S<N>`, `D<N>,<N>`, and `C<N>,<N>` descriptors with complete-token and port-range checks, avoiding `std::stoi` integer overflow exceptions.
- **Touchstone port comments**: Use linear parsing for legacy `Port N = NAME` and vendor-scoped `PortN_COMPONENT_PIN::NET` forms. Preserve the existing physical port names and UTF-8 bytes.
- **Automatic channel suggestions**: Replace regex-based Near/Far and P/N suffix matching with deterministic suffix checks that preserve legacy precedence and avoid regex-stack risk on very long labels.
- **Source filename ports**: Parse `.sNp` extensions without applying a regular expression to the whole filename.

## Compatibility and boundaries

- Touchstone S-parameter 1.0/1.1 and 2.0/2.1 input support remains as documented; this release does not add unsupported Y/Z/H/G matrix conversion.
- Existing valid differential P/N evidence (including `DATAP0/DATAN0`, `CLKP1/CLKN1`) and explicit PowerSI evidence remain subject to the prior mapping validation policy.
- Previously tolerated malformed numeric port tokens may now be rejected or require manual confirmation. This is intentional input validation, not a change in electrical calculations.
- Existing RL/IL/NEXT/FEXT/TDR computation, saved `.siproject` format, temporary cache ownership/cleanup, multi-instance isolation and Windows package layout are not intentionally modified.

## Regression coverage

- Added `v133_parser_hardening` to CTest and packaged validation binaries.
- Tests cover valid and malformed mixed-mode port descriptors, integer overflow, legacy/scoped port labels, invalid zero-based PowerSI indices, physical endpoint overflow, and long UTF-8 label mapping.
- Existing `v130_touchstone_compatibility`, `mapping_and_sign`, `mapping_confirmation_policy` and all other core/GUI tests continue in the platform CI pipeline.

## Validation / release evidence

CI outcomes and download checksums must be taken from the official GitHub Actions run that publishes v1.3.3. Do not label this document as runtime evidence by itself. The official release remains gated on all three platform jobs and package provenance verification.

See [revision history](Revision_History.md), [English user guide](USER_GUIDE_EN.md), [한국어 사용자 설명서](USER_GUIDE_KO.md), and [release workflow](../.github/workflows/release-3platform.yml).
