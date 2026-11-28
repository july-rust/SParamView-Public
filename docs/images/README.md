# User-guide screenshot provenance

The current v1.4.3 documentation temporarily retains historical v1.4.2 synthetic QA screenshots. They are not v1.4.3 captures, and the old Eye controls may include the removed PHY Profile. No real product measurements are included.

Source tag: v1.4.2; exact source commit: `8a7467166d9485c1a3f545c3b744deeff2585d81`.

- 01–03: original native Windows ARM64 packaged-binary validation screenshots from [release CI](https://github.com/july-rust/SParamView-Public/actions/runs/46093773220), artifact ID 15242977815. Main and RL came from workflow selftest; mapping came from open fallback regression.
- 04–07: actual v1.4.2 Qt analysis widgets using the instrumented test harness (only capture hooks added locally, no application or algorithm changes), Linux/Xvfb with Qt from Ubuntu 24.04. [Capture CI](https://github.com/july-rust/SParamView-Public/actions/runs/46100956719).
- 08–11: actual v1.4.2 executable project validation plots using the synthetic example project in the same Linux/Xvfb capture run.
- 07 is the actual combined Eye PNG export; 06 is the visible window. Layout and fonts can vary by OS, DPI and viewport. No physical-platform or hardware measurement claim is made.
- Historical v1.2.1 screenshots remain recoverable in their original tagged source/Git history; they are no longer the current guide illustrations.

| File | Pixel dimensions | SHA-256 |
|---|---:|---|
| `01_Main_Window.png` | 780 × 780 | `b51f1a43f1d5496998f3b1ff9fddff946aec6d0e261e6e03db5ef9d9b2494a15` |
| `02_Channel_Mapping.png` | 768 × 620 | `358e4c7f001ede70d215e4c16cb572d415464b33a756ae6fa56156856a9bab8c` |
| `03_RL_Analysis_Graph.png` | 2400 × 1400 | `958393b4a3ae3de82d4f36b5ae135c94967b00f14f0990f4f7d69e6909bc07b2` |
| `04_ILD.png` | 1480 × 940 | `4159d23270ceea873a818ff6623c00adbe801a6b3c5eac226dbe314646d7677d` |
| `05_ICN.png` | 1480 × 940 | `bdf2634389f7551e3fdc03691bea2ad83f9b11f2b5496e40a0c229b6c0c951f5` |
| `06_Eye_Multi_View.png` | 1480 × 940 | `b4252ba0c70ed57438c77654bc630a313f695e036c4c2594efef1d2cc173cc18` |
| `07_Eye_Multi_Plot.png` | 1078 × 849 | `1584f0a35393144aa067e2a16ac66dddacea7117c5b73a2406026ffa87f254be` |
| `08_IL.png` | 2400 × 1400 | `6e6eeaaf938fc44e86529d610410281957438e7dce3fbe622bdb95ee55047f94` |
| `09_NEXT.png` | 2400 × 1400 | `f10885d3272974d2a4a377da0b246191675fbf8651f54348406a964f004bff18` |
| `10_FEXT.png` | 2400 × 1400 | `8164d25e3219f3a303255957e0e33b07a7486081f441a07923d2eb2c36c3787e` |
| `11_TDR.png` | 2400 × 1400 | `580b49f7328d7d6c584685da648ee2116d00a7dedcefa984b00353236c8ebba7` |

## v1.4.3 candidate evidence (016)

[Native Windows ARM64 validation artifact](https://github.com/july-rust/SParamView-Public/actions/runs/46134402405/artifacts/15259787666), exact commit `2f254f905326c3a20e110804c55e49f4067864d0`. The verified readable `workspace/language_en_2.png` shows the current Eye Diagram without PHY Profile. Separate `analysis-screenshots/` images are actual offscreen captures but show missing glyphs and are not suitable for the guide. The next capture uses the bundled application font explicitly. Keep historical image provenance unchanged until new images are visually inspected.

## Visually inspected v1.4.3 candidate captures

Commit `9ba7146c22d0c3adbfb61acee92d8285b72c821d` passed all nine non-publication jobs ([CI](https://github.com/july-rust/SParamView-Public/actions/runs/46136603711), [CodeQL](https://github.com/july-rust/SParamView-Public/actions/runs/46136603714)). The [native ARM64 evidence archive](https://github.com/july-rust/SParamView-Public/actions/runs/46136603711/artifacts/15259804829) was downloaded and opened: `analysis-screenshots/04_ILD.png`, `05_ICN.png` and `06_Eye_Multi_View.png` are readable 1024 x 768 actual Qt captures with bundled font; the Eye view uses a scrollable third plot and no PHY selector. `07_Eye_Multi_Plot.png` is 264 x 876 with readable glyphs but narrow/truncated titles, not a recommended manual illustration. The archive uses synthetic fixtures, not real S36P board measurements. Actions artifacts expire; checked-in v1.4.2 images remain explicitly historical until binary replacement is possible. Real S36P validation remains pending; no release publication claimed.
