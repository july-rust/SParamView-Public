# SParamView

SParamView is a desktop tool for Touchstone-based S-parameter analysis and visualization.

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

Next official release target: **v1.3.0** (published only after all three-platform CI and native ARM64 gates pass). The previous verified release is [v1.2.5](https://github.com/july-rust/SParamView-Public/releases/tag/v1.2.5).

Supported release targets:
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:
- [English README](README_EN.md)
- [한국어 README](README_KO.md)
- [English User Guide](docs/USER_GUIDE_EN.md)
- [한국어 사용설명서](docs/USER_GUIDE_KO.md)
- [Release notes](docs/Release_1.3.0.md)
- [Revision history](docs/Revision_History.md)

Core analysis includes RL/IL/NEXT/FEXT and Quick TDR workflows. Quick TDR is a band-limited estimate and should not be treated as an instrument-equivalent measurement.

The v1.3.0 source adds Touchstone v1.0/1.1/2.0/2.1 S-parameter compatibility testing, scoped physical port names from vendor comments, and isolated Windows tool/test executables. Release assets are only published after the official CI gates succeed.

License: MIT. See [LICENSE](LICENSE).

Contact: sparamview@gmail.com
