# SParamView

SParamView is a desktop tool for Touchstone-based S-parameter analysis and visualization.

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

Current official release: **[v1.3.1](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.1)**. v1.3.1 hardens independent GUI cache sessions across multiple simultaneously running processes; it preserves the [v1.3.0](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.0) Touchstone compatibility changes and all historical release assets. Validation covers core, Windows x64/ARM64, native ARM64 and macOS Apple Silicon.

Supported release targets:
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:
- [English README](README_EN.md)
- [한국어 README](README_KO.md)
- [English User Guide](docs/USER_GUIDE_EN.md)
- [한국어 사용설명서](docs/USER_GUIDE_KO.md)
- [Release notes](docs/Release_1.3.1.md)
- [Revision history](docs/Revision_History.md)

Core analysis includes RL/IL/NEXT/FEXT and Quick TDR workflows. Quick TDR is a band-limited estimate and should not be treated as an instrument-equivalent measurement.

v1.3.0 adds Touchstone 1.0/1.1/2.0/2.1 **S-parameter** import compatibility tests, scoped vendor-comment physical port labels, conservative `.DiffChannels` channel validation, and Windows auxiliary executable relocation (`tools/`, `tests/`). Y/Z/H/G parameter matrices are not supported as S-parameter input. Official assets, guides, checksums and validation evidence are available at the tagged release; that tag and its published assets remain immutable.

License: MIT. See [LICENSE](LICENSE).

Contact: sparamview@gmail.com
