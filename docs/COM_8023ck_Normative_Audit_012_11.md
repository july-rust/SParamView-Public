# 012_11 — IEEE 802.3ck Annex 120F normative-profile verification audit

**Date:** 2026-11-17 (KST)  
**Repository baseline:** `integration/v1.4.0` at `02d261a538003362cea940f8f6fe6ffd64cbfb77` before this audit  
**Result:** **PARTIAL — 68/68 published-edition text comparison MATCH; official-original authentication BLOCKED**  
**Production IEEE COM qualification:** **NOT EVALUATED**; no changes to the numerical engine, reference manifest or pass/fail gates.

## 0. Second-pass review of the published-edition text (2026-11-17)

**New evidence (after the first-pass record below):** TWO separately uploaded text reproductions identify themselves as complete **IEEE Std 802.3ck-2022**, with different IEEE Xplore license stamps and download dates (2023-02-08 and 2026-01-28). The visible publication has the same standard title, amendment number and publication page sequence as the [official IEEE catalog](https://standards.ieee.org/ieee/802.3ck/7322/). Its printed **pages 236, 237, 238** show all of **Table 120F-8** and **120F.4.1**. The two reproductions agree on the visible technical data; the 2020 IEEE ad-hoc draft independently corroborates the first 18 package/grid entries.

- [Third-party transcription A — Scribd, with 2023 IEEE Xplore license stamp](https://www.scribd.com/document/937723448/8023ck-2022) — **SECONDARY REPRODUCTION, NOT IEEE distribution**.
- [Third-party transcription B — StudyLib, with 2026 IEEE Xplore license stamp](https://studylib.net/doc/28325086/8023ck-2022) — **SECONDARY REPRODUCTION, NOT IEEE distribution**.
- [Official IEEE published standard identity](https://ieeexplore.ieee.org/document/9999414) and [official IEEE GET route](https://standards.ieee.org/ieee/802.3ck/7322/).
- [Official 2024 base-standard errata sheet](https://standards.ieee.org/wp-content/uploads/2024/02/802.3-2022_errata.pdf): enumerates corrections to 30.12 and 78.4, **not** 120F or 93A. This does not exhaust future/other amendments.
- Independent published post-release IEEE discussion [802.3dj Nov 2023 DER0 slide](https://www.ieee802.org/3/dj/public/23_11/healey_3dj_01a_2311.pdf) corroborates Table 120F-8's \`DER0=1e-5\`.

**Critical provenance limit:** The two hosted reproductions are not official IEEE distribution endpoints. Although they present complete IEEE Xplore-watermarked approved-edition text with distinct licensed readers, **an authenticated official IEEE GET PDF and byte SHA-256 have NOT been obtained**. Publication-text numerical agreement is much stronger evidence than the earlier draft comparison, but **not the same as formal original-source authentication**. Neither complete original nor transcription bytes are redistributed in the repository.

### Published-edition value comparison: 68/68 code numeric leaves MATCH in both transcriptions

The independently recorded expected **field values** are pinned by [\`test_com_120f8_published_profile.py\`](../tests/channel_analysis/test_com_120f8_published_profile.py) and are compared mechanically to the actual C++ header. Counts below refer to primitive numeric fields, not IEEE table *rows*; multiple values in a range/array are each counted once. The first 18 draft comparisons remain documented under section 2 for chronology.

| C++ contract part | Numeric leaves | Two published-edition transcriptions | C++ comparison | Published pages |
| --- | ---: | --- | --- | --- |
| Fixed scalar parameters, incl. grid, package, CTLE filter frequencies, PAM4, amplitudes, noise, jitter, detector target, COM criterion and 7.5 ps transition | **34** | Agreement | **MATCH** | 120F.4.1, pp. 236–238 |
| FFE noncursor (4 × min/max/step) and two CTLE ranges (2 × min/max/step) | **18** | Agreement | **MATCH** | p. 237 |
| TX/RX package cases (2 + 2 values) and DFE coefficient limits (6 + 6) | **16** | Agreement | **MATCH** | pp. 236–237 |
| **Total** | **68** | **No visible disagreement** | **68/68 MATCH** | **pp. 236–238** |

Notes on physical interpretation:

1. \`fb=53.125 GBd\` is symbol rate, **not** 53.125 Gb/s raw binary bit rate; \`M=32\` samples/UI; four PAM levels. Report in Hz/Bd with correct units.
2. COM \`>=3 dB\` **for both Test 1 and Test 2** is specifically from **120F.4.1**, not a universal limit. The code's 7.5 ps parameter for \`Ht(f)\` is also from that subclause.
3. Test 1 TX/RX = **13/11 mm**; Test 2 TX/RX = **31/29 mm**. The checked code array order agrees. Do not replace one direction's case with the other.
4. \`Cd\`/\`Cb\`/\`Cp\` are single-ended capacitances in the original table; their C++ \`FaradSingleEnded\` values are numerically consistent after conversion from nF. \`Zc\` and \`Zc2\` match the published package model. This alone does **not** independently verify all network equations and impedance normalization.
5. Four FFE pre/post ranges, two CTLE gain ranges, CTLE zero/poles, six DFE min/max values, TX amplitudes, jitter/noise and DER target all agree between the copies and current header.
6. **Separate procedure caveat:** 120F.3.2.4 receiver-interference-tolerance calibration has *measured transmitter* substitutions for SNRTX, RLM and jitter and selects the **lower** of the two calculated Test 1/2 margins. This is not a license to silently replace the channel-characteristic 120F.4.1 profile defaults with measured calibration values. No receiver-compliance claim was evaluated.

The published-edition source transcriptions do **not** establish that the native C++ solver implements every 93A optimization, ISI, DFE, crosstalk and noise algorithm. They verify numeric *profile candidates only*. The production \`calculateCom()\` remains **NOT EVALUATED**.

### New automated verification

A dedicated regression is registered as CTest \`com_120f8_published_parameter_contract\`, with 68 numeric code assertions and separate fail-closed parser/Test1/Test2 ordering tests. It fails on a changed constant, missing field, altered min/max/step, wrong package case, or an unsupported executable initializer. It uses a restricted arithmetic AST reader and does not execute or download any third-party code or IEEE standards PDF.

**Execution status:** See exact-head PR CI and final merge receipt for the run; no test result is inferred from the presence of the test file.

### Remaining strict final-source gate

The technical *table-content* cross-check has progressed from 18 draft-only to **68 published-edition numeric leaves**. The original 012_11 100%-COMPLETE requirement also calls for direct access to an **authorized/official final original**. Since that was not available, keep **012_11 = PARTIAL (content verified via two untrusted distribution mirrors; authoritative original authentication BLOCKED)**. Do not set \`parameter_values_qualified=true\` merely from secondary reproduction, do not enable IEEE COM PASS/FAIL, and do not relabel historical reference goldens as 802.3ck final.

To close the remaining prerequisite in the **same task**, compare the authorized IEEE GET original's printed pp. 236–238 (including edition and any applicable corrections) to these 68 pins, record provenance and its SHA-256 if the licensed PDF is legitimately available. The numeric-comparison test can then be reused without expanding task scope.

---

## First-pass audit record (before the two published-text reproductions were found)

## 1. Scope and evidence classes

This audit examines only the 100GAUI-1/200GAUI-2/400GAUI-4 C2C COM setting contract (`include/si/channel_analysis/com_120f8.hpp`) and the Test 1/Test 2 identity in the existing implementation. It is **not** an IEEE standard-conformance certification, a complete Annex 93A algorithm review, a numerical golden comparison, or a release gate.

| Evidence | Source | Status | Permissible conclusion |
| --- | --- | --- | --- |
| IEEE Std 802.3ck-2022 identity, publication, amendments | [IEEE SA catalog](https://standards.ieee.org/ieee/802.3ck/7322/), [IEEE Xplore document 9999414](https://ieeexplore.ieee.org/document/9999414) | **OFFICIAL METADATA VERIFIED** | Published 2022-12-28; amendment includes Annex 120F |
| Entire final-approved 120F.4.1 / Table 120F-8 and relevant 93A text | [IEEE GET program](https://ieeexplore.ieee.org/browse/standards/get-program/page/series?id=68) | **UNVERIFIED** | The complete licensed final text/PDF could not be accessed by the audit; **no page-by-page or value-by-value final normative verification is claimed** |
| Historical 2020 Annex 120F parameter proposal, page/slide 17 | [IEEE 802.3ck ad-hoc 2020-09-23 presentation](https://www.ieee802.org/3/ck/public/adhoc/sept23_20/wu_3ck_adhoc_01a_092320.pdf) | **IEEE WORKING-GROUP DRAFT / NOT NORMATIVE** | May cross-check candidate values and SI units but **cannot confirm final Table 120F-8** (the presentation labels its table **120F-7**) |
| Historical 2022 3ck sponsor-ballot COM workbook | [Existing source audit](COM_2022_SA_Reference_Audit_012_09.md) | **HISTORICAL / NOT NORMATIVE** | Workbook SHA/audit does not substitute for the released standard |
| Existing source values | [COM C++ header](../include/si/channel_analysis/com_120f8.hpp) | **IMPLEMENTATION INVENTORY** | Describes what the code currently assumes, not external correctness |
| IEEE 802.3-2022 corrections | [802.3 family catalog](https://standards.ieee.org/ieee/802.3/10422/) | **METADATA ONLY** | Applicable individual errata/paragraph changes to 120F/93A not established by this audit |

**Copyright/source handling:** No licensed IEEE standard text, draft PDF, workbook or third-party channel data is committed. The official IEEE catalog and public IEEE working-group presentation are linked, not copied into the repository.

## 2. Bounded cross-check of 18 historical IEEE working-group values

The entries below are **existing C++ values**, not newly certified normative values. They are compared against the published September 2020 IEEE 802.3ck *working-group presentation*, page/slide 17, where the table was called **120F-7**. Status `DRAFT_MATCH` means a draft cross-check only; final-approved status for **every** entry remains `UNVERIFIED_FINAL`.

| Parameter | Existing C++ value (SI or source unit) | WG 2020 candidate | Draft comparison |
| --- | ---: | ---: | --- |
| Symbol rate | 53.125 GBd | 53.125 GBd | DRAFT_MATCH |
| Maximum start frequency | 0.05 GHz | 0.05 GHz | DRAFT_MATCH |
| Maximum frequency step | 0.01 GHz | 0.01 GHz | DRAFT_MATCH |
| `Cd` single-ended | 1.2e-13 F = 1.2e-4 nF | 1.2e-4 nF | DRAFT_MATCH |
| `Ls` single-ended | 1.2e-10 H = 0.12 nH | 0.12 nH | DRAFT_MATCH |
| `Cb` single-ended | 3.0e-14 F = 3e-5 nF | 3e-5 nF | DRAFT_MATCH |
| Test 1 Tx `zp` | 13 mm | 13 mm | DRAFT_MATCH |
| Test 1 Rx `zp` | 11 mm | 11 mm | DRAFT_MATCH |
| Test 2 Tx `zp` | 31 mm | 31 mm | DRAFT_MATCH |
| Test 2 Rx `zp` | 29 mm | 29 mm | DRAFT_MATCH |
| Secondary package line length | 1.8 mm | 1.8 mm | DRAFT_MATCH |
| `a1` package loss | 9.909e-4 sqrt(ns)/mm | 9.909e-4 sqrt(ns)/mm | DRAFT_MATCH |
| `a2` package loss | 2.772e-4 ns/mm | 2.772e-4 ns/mm | DRAFT_MATCH |
| `Cp` single-ended | 8.7e-14 F = 8.7e-5 nF | 8.7e-5 nF | DRAFT_MATCH |
| Primary package line `Zc` | 87.5 Ω | 87.5 Ω | DRAFT_MATCH |
| Secondary package line `Zc2` | 92.5 Ω | 92.5 Ω | DRAFT_MATCH |
| Reference resistance `R0` | 50 Ω | 50 Ω | DRAFT_MATCH |
| Receiver termination `Rd` | 50 Ω | 50 Ω | DRAFT_MATCH |

**Outcome:** 18/18 candidate fields are consistent with this **historical** public working-group table; **0/18 are certified as final-standard verified**. This draft comparison **does not** establish that no parameters changed before the 2022 approved release.

## 3. Additional current-code inventory requiring approved-final verification

The following are read directly from `Com120F8Parameters` and are **unverified against the released Table 120F-8**:

| Group | Existing code candidate | Approved-final status |
| --- | --- | --- |
| COM criterion and transition | `minimumComDb=3.0`, `txTransitionTimeS=7.5e-12` | UNVERIFIED_FINAL |
| Receiver bandwidth | `0.75 * 53.125e9 Hz` | UNVERIFIED_FINAL |
| FFE cursor and noncursor | `c(0)>=0.54`; `c(-3)=-0.04..0 step .02`; `c(-2)=0..0.10 step .02`; `c(-1)=-0.28..0 step .02`; `c(+1)=-0.10..0 step .02` | UNVERIFIED_FINAL |
| CTLE DC-gain grids | `gDC=-20..0 dB step 1`; `gDC2=-4..0 dB step 1` | UNVERIFIED_FINAL |
| CTLE poles/zero | `fb/2.5`, `fb/2.5`, `fb`, `fb/80` | UNVERIFIED_FINAL |
| Peak differential TX voltages | victim/FEXT `0.413 V`; NEXT `0.608 V` | UNVERIFIED_FINAL |
| PAM4 / RLM / TX noise | levels `4`; `RLM=0.95`; `SNRTX=33 dB` | UNVERIFIED_FINAL |
| Sampling / DFE | `M=32`; `Nb=6`; min/max tap arrays as encoded in header | UNVERIFIED_FINAL |
| Jitter / noise / detector target | `randomJitterRmsUi=0.01`; `dualDiracJitterPeakUi=0.02`; `oneSidedNoiseSpectralDensityV2PerGHz=2e-8`; `targetDetectorErrorRatio=1e-5` | UNVERIFIED_FINAL |

This table is an **inventory of code claims**, not an assertion that each field is an exact official Table 120F-8 row. Table completeness (including notes, exceptions, parameter naming, mandatory constraints and whether each code field is normative) remains **UNKNOWN** until the approved source is reviewed.

## 4. Test 1 / Test 2 comparison

The existing profile uses `txPackageLengthMm{13,31}` and `rxPackageLengthMm{11,29}` for Test 1 and Test 2, respectively. These **four** values match the 2020 IEEE working-group presentation. The separate cases are explicitly carried through the current physical-channel/loaded-spectra interfaces (see `com_physical_channel.hpp` and `com_120f8.hpp`).

**What this proves:** no apparent Test 1/Test 2 line-length mixing in the *declarative profile* compared with the specified **2020 draft**. **What it does NOT prove:** final 2022-approved package semantics, the complete per-case reference algorithm, the final required COM pass rule, or any executed COM margin.

## 5. Finding and release-safety disposition

- **F-11-01 (BLOCKER):** The official final approved Table 120F-8 PDF and line/page references were not available for a complete audited comparison. The IEEE Xplore catalog was readable but the final text retrieval endpoint required interactive access. The standard's identity is verified, *not* its whole parameter table.
- **F-11-02 (BLOCKER):** No approved-final independent source audit for the remaining FFE/CTLE/jitter/DFE/noise configuration, table footnotes or 93A/120F exceptions. Historical workbook and early draft agreement cannot close this.
- **F-11-03 (PENDING):** The header describes `final8023ck120F8()` as final/published without a completed page-by-page normative citation. Preserve current fail-closed production COM behavior; do **not** treat this identifier, code comment or unit-test name as qualification evidence.
- **F-11-04 (PENDING):** Existing COM golden correlations were executed under historical or non-802.3ck candidates; none can close 012_11 normative confirmation or authorize 012_12 approved-final goldens.

**Safety decision:** Do not modify numerical defaults, `calculateCom()`, the `REFERENCE_READY` manifest, `parameter_values_qualified=false`, or the disabled normative PASS/FAIL status. In particular, passing C++ component tests does not change source certification status.

## 6. Completion gate / next action

`012_11` **cannot be marked 100% COMPLETE** on the available approved-final evidence. To close **this same task**, obtain the legally accessible final IEEE 802.3ck-2022 text through IEEE GET or an authorized source; record exact edition, page/table, SHA-256 (if the binary is available), every applicable final Table 120F-8 line and footnote, Test 1/2 and 93A.1 exception, then perform a source-to-code comparison. If any value differs, amend only the minimal parameter contract after licensed-source review and corresponding targeted tests. **Do not create an additional numbered command** for this missing prerequisite.

The historical 2020 table is intentionally a bounded independent sanity check, not a sufficient replacement. Until official verification is complete, the next approved-final COM numeric golden/production verdict gate is **BLOCKED**. Existing generic ILD/ICN/Eye results are unaffected.

### Actual execution and tests

- GitHub connector: fetched the current `integration/v1.4.0` header, `com_120f8.cpp`, reference manifest and prior COM audits.
- Official IEEE SA/Xplore: confirmed the published amendment metadata; full normative text unavailable through this interface.
- IEEE working-group public draft: read the 2020 parameter candidate; 18 current header fields were manually compared with the published draft table as listed above.
- No modified production code; no C++ tests or full-platform binaries executed as part of this documentation-only audit.
- PR/CI merge receipts must be attached when available; **this text alone is not CI evidence**.
