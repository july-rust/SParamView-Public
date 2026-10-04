# SParamView

SParamView is a desktop tool for Touchstone-based S-parameter analysis and visualization.

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

Current public source baseline: **v1.2.1**

Supported release targets:
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:
- [English README](README_EN.md)
- [한국어 README](README_KO.md)
- [English User Guide](docs/USER_GUIDE_EN.md)
- [한국어 사용설명서](docs/USER_GUIDE_KO.md)
- [Release notes](docs/Release_1.2.1.md)

Core analysis includes RL/IL/NEXT/FEXT and Quick TDR workflows. Quick TDR is a band-limited estimate and should not be treated as an instrument-equivalent measurement.

The public repository contains the validated release-oriented source snapshot. Development history remains in a separate private repository.

License: MIT. See [LICENSE](LICENSE).

Contact: sparamview@gmail.com
