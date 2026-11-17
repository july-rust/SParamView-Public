# 012_11 — IEEE 802.3ck Annex 120F normative-profile verification audit

**Date:** 2026-11-17 (KST)  
**Repository baseline:** `integration/v1.4.0` at `02d261a538003362cea940f8f6fe6ffd64cbfb77` before this audit  
**Result:** **PARTIAL / BLOCKED — NOT 100% COMPLETE**  
**Production IEEE COM qualification:** **NOT EVALUATED**; no changes to the numerical engine, reference manifest or pass/fail gates.

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
