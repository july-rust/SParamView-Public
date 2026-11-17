# 012_13 — IEEE 802.3ck-2022 COM reference numerical qualification

**Result: 012_13 REFERENCE NUMERICAL QUALIFICATION COMPLETE (2026-11-17 KST) — six source-locked, explicitly defined reference candidate cases.** This is **not** IEEE PHY certification or native SParamView C++ COM qualification. Do not promote the source-free numerical Golden to a production PASS/FAIL.

## 1. Source and model provenance

- **Normative numerical source**: 316-page licensed IEEE Std 802.3ck-2022 original, Annex 120F.4.1/Table 120F-8, printed pp. 236–238. Original SHA-256 `5f5db5a7e94e49cdc68a18db63b7ec01b349467f680251194fbc0a1295faa1e9`; all 68 independently counted C++ numeric leaves were verified in [012_11](COM_8023ck_Normative_Audit_012_11.md). Licensed PDF not redistributed.
- **Numerical contract**: 53.125 GBd (not raw bit rate); TX transition 7.5 ps; Test 1 TX/RX package 13/11 mm, Test 2 31/29 mm, second segment 1.8 mm. Source values are deterministically loaded from the `012_12` verified configuration; no source-verified numeric leaf is overwritten.
- **Explicit non-normative aggressor model**: Original `012_12` parser-qualification CSV deliberately used `NO_COM=1` and zero-length NEXT/FEXT package sentinels. A **separate temporary numerical CSV** replaces *only* these QA sentinels with engineering geometry documented in [IEEE P802.3ck March 2020 slide 10](https://www.ieee802.org/3/ck/public/20_03/sun_3ck_01_0320.pdf): NEXT `[11 29;1.8 1.8]` mm, FEXT `[13 31;1.8 1.8]` mm and package delay 0.006141 ns/mm. **This 2020 historical model is NOT an explicitly approved final Table 120F-8 parameter.** The original safe configuration is unchanged.
- **Actual public S4P source**: [IEEE 2019 C2C ZIP](https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip), SHA-256 `670fe10711b5c530320f770d22a717387b161f2721a2203b27e84ebf7ac597e2`. Real THRU/FEXT/NEXT originals are SHA-verified by member. Physical source port mapping `[1 2 3 4]` is explicitly validated. The historical fixture uses **4 FEXT instances of one original FEXT file and 0 NEXT**; do not misrepresent these as four independent measured FEXT originals.
- **Independent implementations**: SiCoPR Python and separately executed Octave-compatible COM 4p17p0, both from pinned `tbermen/SiCoPR@cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75`. Compatibility source is official COM v4.17 Git commit `47459695a169fdf1fd10c541a5b19c69a5ae9285`; **official original MATLAB source was not executed directly**. Input SHA and result identity are checked for both engines.

## 2. Actual six-case COM and FOM results

The following are real finite results from the NEW `012_13` source-verified numerical runs, **not** from `012_12` CSV-only parser tests, with two independent calculations of each Test 1/2. Comparisons enforce identical effective CSV SHA and original IEEE channel member fingerprints.

| Historical channel | Test | Python COM (dB) | Octave COM (dB) | Python FOM (dB) | Octave FOM (dB) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 10 dB | 1 | 4.8425407043560735 | 4.84254070435603 | 17.359998002530823 | 17.35999800253053 |
| 10 dB | 2 | 4.697201007609777 | 4.697201007609771 | 17.244092226424666 | 17.244092226424662 |
| 16 dB | 1 | 4.293845073865506 | 4.293845073865389 | 16.825974111052055 | 16.825974111051995 |
| 16 dB | 2 | 3.9994128151173114 | 3.999412815117313 | 16.532274640085806 | 16.532274640085788 |
| 18 dB | 1 | 4.054649183385672 | 4.05464918338567 | 16.560322876677272 | 16.560322876677603 |
| 18 dB | 2 | 3.5828402112060016 | 3.5828402112060003 | 16.108377606388284 | 16.10837760638834 |

**Independent tolerance = 0.0001 dB** each for COM and FOM; **measured maximum COM deviation 1.1723955140041653e-13 dB**, **measured maximum FOM deviation 3.304023721284466e-13 dB**. All six pass. Any missing, NaN, infinite, nonreal, phase-case-misaligned or numerically inconsistent output is rejected. **A COM value numerically above 3 dB here does NOT qualify the actual PHY under 802.3ck**, because the supplemental 2020 aggressor model and 2019 fixture are not final-certified PHY compliance channels.

Durable source-free results: [012_13 Golden candidate JSON](../tests/fixtures/channel/com_reference/012_13_numerical_golden_candidate.json). This contains exactly 6 measured COM/FOM pairs, provenance, explicit non-normative flags, and maxima.

## 3. Reproducible execution evidence

- **[GitHub Actions numeric reference CI #44070019965](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965)**: offline source+negative contract tests **PASS**, pinned real SiCoPR Python on 3 historical source groups **PASS**, separate Octave COM 4p17p0 3-group jobs **PASS**, and independent Python-versus-Octave COM/FOM correlation for **all 3 × 2 cases PASS**.
- Executable sources: `scripts/com_120f8_reference_candidate.py` and `scripts/com_120f8_reference_compare.py`. Corresponding negative tests: `tests/channel_analysis/test_com_120f8_reference_candidate.py` and `tests/channel_analysis/test_com_120f8_reference_compare.py`.
- CI entrypoint: [com-120f8-real-numerical-reference.yml](../.github/workflows/com-120f8-real-numerical-reference.yml). It pins the external checkout and source ZIP/member hashes, enforces per-run bounds, runs each 2-case computation, checks identical effective CSV fingerprints, and only uploads **source-free JSON receipts**. It never commits or uploads IEEE licensed PDF, raw IEEE ZIP/workbook/S4P or third-party implementation.
- Reproduction offline: `python -m unittest discover -s tests/channel_analysis -p 'test_com_120f8_reference_candidate.py' -v`, similarly `test_com_120f8_reference_compare.py`. Running only these local tests is NOT equivalent to having actually run the independent COM solvers.
- Real computation in an ephemeral runner with pinned reference checkout: `python scripts/com_120f8_reference_candidate.py --channel 10 --mode python --external _external_qa_sicopr --output python.json`; repeat with `--mode octave` and compare with `python scripts/com_120f8_reference_compare.py --channel 10 --python-result python.json --octave-result octave.json --output correlated.json`. Repeat separately for 16 and 18.

## 4. Strict completion scope

**012_13 completed:** real 3-channel × 2-package COM and FOM reference candidate numerics, exact 68-leaf final parameter source qualification, input role/ZIP/S4P SHA and physical port evidence, repeatable dual-engine comparison, bounded CI, negative fail-closed cases and source-free durable Golden metadata. No additional task numbers and no duplicated platform documentation.

**Not claimed:** final published PHY-certified aggressor topology, actual official MATLAB runtime execution, source-certified final-PHY real channel corpus, native SParamView C++ full 93A/120F COM optimizer or production PHY PASS/FAIL. Those were outside the bounded reference-numerics-only `012_13` scope and need dedicated later qualification. Production `calculateCom()` remains `NotEvaluated`, manifest `REFERENCE_READY` with `standard.parameter_values_qualified=false`, official 3 dB PASS/FAIL disabled. **Do not alter `main`, v1.3.5, release/tag assets, or pretend numerical reference validation is production qualification.**
