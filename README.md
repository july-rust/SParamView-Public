# SParamView

SParamView is a desktop tool for Touchstone-based S-parameter analysis and visualization.

> **Important Notice**
> SParamView calculates and visualizes the provided Touchstone data as a reference tool. Depending on the input data, frequency range, port/channel mapping, interpolation or approximation, and other analysis conditions, displayed graphs and calculated results may differ from the actual system or measurement results. Do not treat the output as 100% accurate or authoritative. Use it as a reference for engineering review, and independently verify important decisions against the original data and trusted measurement or analysis results.

Current official release: **[v1.4.3](https://github.com/july-rust/SParamView-Public/releases/tag/v1.4.3)**. Independent ILD, ICN and Eye Diagram windows, runtime language switching and GUI improvements; COM deferred.

Supported release targets:
- Windows x64
- Windows ARM64
- macOS Apple Silicon (arm64)

Documentation:
- [Documentation index — current guides, release evidence and historical records](docs/README.md)
- [English README](README_EN.md)
- [한국어 README](README_KO.md)
- [English User Guide](docs/USER_GUIDE_EN.md)
- [한국어 사용설명서](docs/USER_GUIDE_KO.md)
- [Release notes](docs/Release_1.4.3.md)
- [Revision history](docs/Revision_History.md)
- [Historical channel-analysis development plan (COM deferred)](docs/Channel_Analysis_Development_Plan.md)
- [Development history and release closeout (012_62)](docs/Development_Plan_012_v1.4.0.md)

### What's new in v1.4.3

- Successful Auto Analysis/Auto Preview fills Manual controls with resolved settings. Unedited frequency and ICN numeric controls retain calculation precision when another field is changed. Review assumptions before applying known conditions. PHY Profile is removed; Eye overlay includes TX incident voltage as a visual reference. COM remains deferred.

### What's new in v1.4.2

- Eye Diagram now offers **All three graphs** alongside the existing individual views: Eye overlay (2 UI), TX / RX waveform and Impulse / Unit pulse. All panels share one accepted numerical result, use independent axes and interaction, and support PNG export of the selected view. The combined layout adapts to logical width; narrow windows can scroll. The existing simulation-only qualification limits remain unchanged.

### What's new in v1.4.1

- **v1.4.1:** File > New closes ILD, ICN and Eye Diagram only after confirmation; Cancel or a failed save preserves the workspace. File > Exit uses the normal save/cleanup path. Window geometry is fitted to the active monitor's logical work area, with scrollable custom dialog bodies and fixed confirmation buttons. Qt file dialogs are used consistently across platforms.

### What's new in v1.4.0

- Independent ILD, Generic ICN and simulated NRZ Eye Diagram analysis windows. Select the mapped victim/path; use Auto Analysis/Auto Preview for initial conditions or Run Manual for known conditions. Review assumptions, warnings, and quality status; export CSV/PNG.
- ILD displays measured/fitted insertion loss and deviation. ICN integrates mapped NEXT/FEXT paths with explicit signal/filter and independent-path RSS assumptions. Eye Diagram is a band-limited simulation, not measured BER. Auto settings are engineering assumptions, not inferred PHY settings.
- Eye Diagram checks DC quality. For missing DC, start frequency up to 10 kHz has no frequency-based confirmation, above 10 kHz and below 100 kHz requires confirmation, and at or above 100 kHz is blocked; other quality checks still apply. Conditional AC-like one-pole extrapolation does not establish physical topology or long-pattern accuracy.
- Runtime English/Korean/Japanese language switching; Language menu stays in English. New resets the workspace after unsaved-change confirmation; multiple Touchstone files are processed sequentially with individual mapping. Last directory and main-window state are remembered. Analysis windows adapt to the display; guidance scrolls independently of plots. Table refresh, overlay and rendering are optimized.
- Default limits: RL -10 dB, IL -3 dB, NEXT/FEXT -20 dB, TDR -10%; checks initially off. These are configurable examples, not standards.
- Windows user ZIP excludes optional test executables and verification launcher; separate QA ZIP supports validation.

**Scope:** engineering-reference calculations only. COM is deferred and has no production menu. No IEEE/SNIA/MIPI compliance, measured BER or exhaustive physical-platform/HiDPI certification is claimed.

Core analysis includes RL/IL/NEXT/FEXT and Quick TDR workflows. Quick TDR is a band-limited estimate and should not be treated as an instrument-equivalent measurement.

v1.3.0 adds Touchstone 1.0/1.1/2.0/2.1 **S-parameter** import compatibility tests, scoped vendor-comment physical port labels, conservative `.DiffChannels` channel validation, and Windows auxiliary executable relocation (`tools/`, `tests/`). Y/Z/H/G parameter matrices are not supported as S-parameter input. Official assets, guides, checksums and validation evidence are available at the tagged release; that tag and its published assets remain immutable.

License: MIT. See [LICENSE](LICENSE).

Contact: sparamview@gmail.com

### Mapping CSV v3 compatibility

Mapping CSV v3 (`v3-text-percent`) prefixes all four text columns with `text:` and percent-encodes whitespace, controls and `%`. Examples: `00123` → `text:00123`, newline → `%0A`, literal `%` → `%25`. Port columns remain integers. Preserve the prefix and escapes when editing CSV directly; prefer the Channel Mapping editor. Legacy v1/v2 files remain readable, but **v1.3.4 cannot read new v3 files**. See [qualification scope](docs/Final_Qualification_1.3.5.md) for actual Linux Calc GUI evidence and Excel/platform limitations.
