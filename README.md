# SParamView

SParamView is a desktop application for viewing and analyzing Touchstone S-parameter data, including channel-oriented RL/IL/NEXT/FEXT analysis and Quick TDR.

**Current public release target: v1.1.5**

Supported release platforms:

- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:

- [English](README_EN.md)
- [한국어](README_KO.md)
- [v1.1.5 Release Notes](docs/Release_1.1.5.md)

## Build

The project uses C++20, CMake, and Qt 6. The release CI uses Qt 6.8.3 and validates the common core, Windows x64, Windows ARM64, and macOS Apple Silicon builds before release.

## License

SParamView application source is licensed under the MIT License. Third-party components and bundled assets retain their respective licenses; see `third_party/` and `COPYRIGHT.txt`.

## Release policy

This repository is the public stable-release repository. Development and experimental history are maintained separately. Public releases are merged only after the staging branch passes the configured validation gates.
