# SParamView 1.2.1

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

## v1.2.1
- Stabilized result ranking when defined and missing margins are mixed.
- Limited Quick TDR suitability checks to the selected time range.
- Includes the input-validation and TDR viewing improvements from v1.1.6/v1.1.7 and the source partitioning from v1.2.0.
- Added regression coverage for result ordering and TDR range suitability.

See [Release_1.2.1.md](docs/Release_1.2.1.md) for details. Earlier changes: [v1.1.6](docs/Release_1.1.6.md), [v1.1.7](docs/Release_1.1.7.md), [v1.2.0](docs/Release_1.2.0.md).

## Build
SParamView uses C++20, CMake, and Qt 6.8.3 (Core, Gui, Widgets, Concurrent).

The GitHub Actions workflow performs version consistency checks, common regression tests, sanitizer checks, Windows x64/ARM64 builds, a native Windows ARM64 packaged-binary gate, and an Apple Silicon macOS build/test gate.

## License
SParamView source is distributed under the MIT License. See [LICENSE](LICENSE).
Third-party notices and license texts are stored under `third_party/`.

## Contact
sparamview@gmail.com
