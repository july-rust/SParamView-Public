# SParamView 1.1.5

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

## v1.1.5
- Added **Open Project** and **Save Project** buttons next to **+ Open Touchstone**.
- Reused the existing project load/save paths; the Touchstone parser and validated analysis equations are not intentionally changed by this release.
- Added regression coverage for the new header buttons.
- Standard release targets are Windows x64, Windows ARM64, and macOS Apple Silicon (arm64).

See [Release_1.1.5.md](docs/Release_1.1.5.md) for the release scope.

## Build
SParamView uses C++20, CMake, and Qt 6.8.3 (Core, Gui, Widgets, Concurrent).

The GitHub Actions workflow performs version consistency checks, common regression tests, sanitizer checks, Windows x64/ARM64 builds, a native Windows ARM64 packaged-binary gate, and an Apple Silicon macOS build/test gate.

## License
SParamView source is distributed under the MIT License. See [LICENSE](LICENSE).
Third-party notices and license texts are stored under `third_party/`.

## Contact
sparamview@gmail.com
