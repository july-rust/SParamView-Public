# SParamView v1.1.5

## Important notice

SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Important engineering decisions should be independently verified against the original data and trusted measurement or analysis results.

## UI

- Added **Open Project** and **Save Project** buttons to the upper-right header, immediately after **+ Open Touchstone**.
- The new buttons reuse the existing `.siproject` load/save paths; Touchstone parsing and analysis algorithms are unchanged.
- Added a GUI regression test that verifies button presence, labels, ordering, and idempotent installation.

## Platforms

Starting with v1.1.5, the standard release set is:

- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

The release workflow validates the version across the core header, CMake project metadata, and Windows VERSIONINFO before building. Windows ARM64 is cross-built and then executed on a native ARM64 GitHub-hosted runner. macOS is built and tested on an Apple Silicon GitHub-hosted runner and the packaged Mach-O files are audited to contain arm64.

## Compatibility and scope

- Existing project file handling is reused; no project schema change is introduced by this release.
- Existing Touchstone analysis calculations are not intentionally modified by this UI/platform release.
- macOS packages are ad-hoc signed and are not notarized unless a future signing/notarization workflow is explicitly configured.
