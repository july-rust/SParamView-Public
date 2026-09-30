# SParamView v1.1.5

SParamView is a C++20 / Qt 6 desktop application for viewing and analyzing Touchstone S-parameter data.

## Main functions

- Open Touchstone files and `.siproject` project files
- Channel mapping for single-ended and differential analysis
- Return Loss (RL / S11 / Sdd11)
- Insertion Loss (IL / S21 / Sdd21)
- NEXT and FEXT analysis
- Quick TDR visualization
- Result-table review and export workflows

Quick TDR is a band-limited estimate and should not be treated as a replacement for an instrument measurement.

## v1.1.5

- Added **Open Project** and **Save Project** buttons to the upper-right header next to **+ Open Touchstone**.
- Existing project load/save paths are reused; no project schema change is introduced.
- Existing Touchstone parsing and analysis calculations are not intentionally modified by this UI/platform release.

See [v1.1.5 Release Notes](docs/Release_1.1.5.md) for the release scope.

## Supported release platforms

- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

The release CI validates version consistency before building. Windows ARM64 is cross-built and then exercised on a native ARM64 runner. macOS is built and tested on an Apple Silicon runner.

The macOS package is currently ad-hoc signed and is not notarized.

## Build

Requirements:

- C++20 compiler
- CMake
- Qt 6 development packages: Core, Gui, Widgets, Concurrent

The release workflow currently uses Qt 6.8.3.

Typical CMake flow:

```text
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Platform-specific packaging and validation are implemented under `.github/workflows/` and `scripts/`.

## Interpretation notes

- RL/IL labels are shown together with their S-parameter notation where applicable.
- NEXT is the near-end victim response for a near-end aggressor; FEXT is the far-end victim response for a near-end aggressor.
- OK means the enabled limit checks passed; NG means a violation was detected; N/A is not a pass.
- Differential P-N 100-ohm termination and separate P-GND/N-GND 50-ohm termination are distinct configurations.

## Example data

The files under `examples/` are synthetic demonstration fixtures generated for regression and UI workflows; they are not measurement data from a product.

## License

SParamView application source is licensed under the MIT License. Third-party components, runtime notices, and fonts retain their respective licenses. See `LICENSE`, `COPYRIGHT.txt`, and `third_party/`.
