# SParamView v1.3.1

SParamView is a desktop application for Touchstone-based S-parameter analysis and visualization.

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

## Release targets
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

## Main analysis functions
- Return Loss (RL / S11, Sdd11)
- Insertion Loss (IL / S21, Sdd21)
- NEXT
- FEXT
- Quick TDR
- Project load/save (`.siproject`)

Quick TDR is a band-limited estimate affected by frequency span, spacing, extrapolation, windowing, and termination. It should not be interpreted as an instrument-equivalent measurement.

## User guide

- [English User Guide](docs/USER_GUIDE_EN.md)
- [한국어 사용자 설명서](docs/USER_GUIDE_KO.md)

## Current release — v1.3.1

**[Official release and downloads](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.1)** · [v1.3.1 release notes](docs/Release_1.3.1.md) · [release CI](https://github.com/july-rust/SParamView-Public/actions)

- **v1.3.1:** Multiple running SParamView processes keep private cache files; shutdown cleanup is serialized with abandoned-session recovery. Separate instances never delete each other's active cache. Opening other Touchstone files *inside the same window* still appends revisions (see Issue #15).
- **v1.3.0:** Import Touchstone **1.0/1.1** (implicit version) and **2.0/2.1** (explicit `[Version]`) **S-parameter** files, with RI/MA/DB, standard frequency units, and supported matrix layouts.
- Recognize `! PortN_COMPONENT_PIN::NET` vendor comments as physical port labels; validate `!.DiffChannels` endpoint/NET/P-N mapping against physical ports instead of guessing polarity.
- Re-import with new cache schema so previously missing physical labels are not reused.
- Windows: keep only `SParamView.exe` in the package root, with required Qt/runtime DLLs alongside it. Move CLI to `tools/si_cli.exe` and test executables to `tests/`.
- Y/Z/H/G Touchstone network matrices are **not** accepted as S-parameter input. Touchstone `1.9` is not a defined official version.

## Earlier release — v1.2.5
- Automatically remove each GUI instance's Touchstone conversion cache and Excel report staging files after graceful exit.
- Reclaim abandoned GUI cache sessions on subsequent launches, with ownership checks and multi-instance leases.
- Remove default CLI command caches, preserving explicitly selected cache directories.
- Preserve original Touchstone files, saved projects, reports and UI settings.

## Previous v1.2.4a changes
- Reuse results only when the exact selected analysis jobs match.
- Preserve Single-ended review choices through mapping and project restoration.
- Verify complete source SHA-256 on project restore and rebuild candidates after changes.
- Track saved settings consistently and protect unsaved projects on switching.
- Support additional option-line handling and symmetric mixed-mode triangular matrices in Touchstone 2.1.
- Export analysis-time signal/mapping/source evidence and publish immutable, verified packages.
- Includes the compact v1.2.2 controls and v1.2.3 signal detection changes.

The previous [v1.2.4a published artifacts](https://github.com/july-rust/SParamView-Public/releases/tag/v1.2.4a) remain unchanged. On 2026-10-13, a separate documentation and CI maintenance pass verified zero warning messages across the six required validation gates, native Windows ARM64 mapping fallback 10/10 passes, and native Cocoa macOS GUI tests. These changes do not replace the tagged binaries. See [Revision History](docs/Revision_History.md) and [CI evidence](https://github.com/july-rust/SParamView-Public/actions/runs/38383938009).

See [Release_1.3.1.md](docs/Release_1.3.1.md) for the current release, and [Release_1.3.0.md](docs/Release_1.3.0.md) for its Touchstone predecessor and [Release_1.2.5.md](docs/Release_1.2.5.md) / [Release_1.2.4a.md](docs/Release_1.2.4a.md) for historical details. Earlier changes: [v1.1.6](docs/Release_1.1.6.md), [v1.1.7](docs/Release_1.1.7.md), [v1.2.0](docs/Release_1.2.0.md).

## Build
SParamView uses C++20, CMake, and Qt 6.8.3 (Core, Gui, Widgets, Concurrent).

The GitHub Actions workflow performs version consistency checks, GCC Release core regression tests, Clang ASan/UBSan checks, Windows x64/ARM64 builds, a native Windows ARM64 packaged-binary gate (including repeated manual-mapping tests), and Apple Silicon macOS builds with Cocoa GUI tests. Release artifact retrieval uses authenticated GitHub CLI. These tests do not imply hardware measurement acceptance.

## License
SParamView source is distributed under the MIT License. See [LICENSE](LICENSE).
Third-party notices and license texts are stored under `third_party/`.

## Contact
sparamview@gmail.com
