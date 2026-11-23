# 012_32 — IEEE 802.3ck-2022 COM final PHY reference-input eligibility audit

**Audit date:** 2026-11-23 (KST)  
**Target:** SParamView v1.4.0, `integration/v1.4.0`  
**Scope:** reference-input **eligibility and provenance**, not C++ COM implementation or the six-case final numerical qualification.  
**Overall reference decision:** **REFERENCE BLOCKED** (evidence-based); do not enable production IEEE COM or PHY PASS/FAIL.  
**Audit delivery gate:** independent PR HEAD CI/review, protected squash merge and post-merge CI must be checked before declaring **012_32 COMPLETE (audit)**.  

## 1. What this audit actually verifies

This is a bounded follow-up to the [reviewed 012_31–50 roadmap](Development_Plan_012_v1.4.0.md) ([plan PR #95](https://github.com/july-rust/SParamView-Public/pull/95)). The planning PR and a source-verified 68-value table are **not** by themselves a finished equation/function Gap Matrix or an approved reference corpus. The normative equation-to-implementation lock and any newer release assessment remain explicit prerequisites of later work if not evidenced by 012_31.

Evidence examined in the current integration branch:

- [012_11 normative-source audit](COM_8023ck_Normative_Audit_012_11.md): independently checked **68/68 existing C++ numeric leaves** against the licensed complete IEEE Std 802.3ck-2022 original, but did not qualify the COM solver. Original licensed PDF SHA-256: `5f5db5a7e94e49cdc68a18db63b7ec01b349467f680251194fbc0a1295faa1e9`. **The licensed PDF was not opened or re-hashed in this audit**; this is a traced prior result.
- [012_02 reference qualification](COM_Reference_Qualification_012_02.md): official 802-COM 4.17.0 source, revision and license evidence; its `REFERENCE_READY` flag is only **algorithm/reference readiness**, not final PHY qualification.
- [012_13 numerical candidate report](COM_120F8_Reference_Numerical_012_13.md), [candidate fixture](../tests/fixtures/channel/com_reference/012_13_numerical_golden_candidate.json), [candidate runner](../scripts/com_120f8_reference_candidate.py) and [source extractor](../scripts/com_3ck_c2c_real_reference.py): historical source-locked six-case candidate, explicitly non-normative aggressor overlays.
- [012_12 config contract](../scripts/com_120f8_config_contract.py): 68-value source lock; `NO_COM=1`, zero-package NEXT/FEXT sentinels and `aggressor_package_and_s4p_compliance=NOT_QUALIFIED` intentionally prevent silently qualifying a parser-test configuration.
- [strict preflight](../scripts/com_reference_preflight.py): `reference-ready` checks a weaker, algorithm-only contract; `qualify` requires separate valid evidence and real C++ producer outputs, absent from the current candidate.

**Evidence level:** repository source/fixture/previous pinned CI receipts were inspected; **no new execution of the licensed IEEE MATLAB program, source ZIP download/re-hash or six-case COM numerical computation is claimed here**. Live external licenses and final PHY approval for every source fixture are not inferred from public HTTP availability.

## 2. Published PHY contract versus candidate engineering adapter

| Requirement | Evidence and decision |
| --- | --- |
| Published standard and applicability | IEEE Std 802.3ck-2022, 120F.4.1, Table 120F-8 and applicable 93A rules; **READY for published 68-value numeric source**, not ready for full channel/reference input |
| Symbol rate | **53.125 GBd**, symbol rate not a raw bit-rate substitute; existing 012_11/12 source lock |
| TX transition | **7.5 ps**, 120F.4.1; existing source lock |
| Test1 package | TX **13 mm**, RX **11 mm**, secondary **1.8 mm**; source-locked numeric values |
| Test2 package | TX **31 mm**, RX **29 mm**, secondary **1.8 mm**; source-locked numeric values |
| Reference-plane, ordering, physical port | Input source port order `[1,2,3,4]` was checked by historical runner; final **TX/RX package and aggressor topology equivalence** for both Test cases still needs the 012_33 fixed-response proof |
| NEXT/FEXT package overlay | Candidate replaces safe `z_p (NEXT)=[0 0;0 0]`, `z_p (FEXT)=[0 0;0 0]` with NEXT `[11 29;1.8 1.8]` mm and FEXT `[13 31;1.8 1.8]` mm, plus `package_tl_tau=0.006141` ns/mm; sourced to **March 2020 P802.3ck slide 10**, not verified as final approved 120F settings: **BLOCKED** |
| Reference runtime controls | Candidate changes QA-only `NO_COM=1` to `NO_COM=0`, explicitly adds TDR/ERL and output controls. This enables a **historical candidate calculation**, not a normative PHY test |
| RX statistics / symbol-voltage and normalization | Some table values are locked; **complete reference-plane and statistical-source ownership across the final 93A calculation is not qualified**. Do not transfer exploratory receiver-voltage/tail bounds to certified FOM/DER |
| Test1 and Test2 acceptance | The 120F.4.1 COM criterion must apply to **each separate case**; neither mean, better case nor source-free candidate value can serve as production PASS |

The 120F.3.2.4 receiver interference-tolerance calibration is a **different test** with measured-input substitutions. It must not be mixed with the 120F.4.1 channel-COM fixed profile.

## 3. Exact original channel inventory (2019 historical IEEE C2C source)

Archive: [IEEE 2019 C2C ZIP](https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip), previously verified SHA-256  
`670fe10711b5c530320f770d22a717387b161f2721a2203b27e84ebf7ac597e2`.

The following are **repository-pinned member SHA-256 values**, taken from `scripts/com_3ck_c2c_real_reference.py`. The SHA-based ephemeral extraction and use in real reference jobs was previously tested in [Actions #44070019965](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965). They are not new network-download hashes from 012_32.

| Historical channel label | THRU .s4p SHA-256 | FEXT .s4p SHA-256 | NEXT .s4p SHA-256 |
| --- | --- | --- | --- |
| 10 dB | `c6ca6b40f66ba887ca8c7962d188a744e2ec9213c8e7589a3ae51218a6e93c51` | `2a67eb48a6c4b8718cc9ad1ae987682a0f37b600a64cfeeeada971747a26b2ee` | `f7d1ae40342dc8379d488e91f5231314c3e469159822aec44819cf78974dbb4c` |
| 16 dB | `7e24b53b88f2bf30ed49c924f330c0bb181a453d39b825cda9190c52ed2c76e6` | `abfe71ac22fa959d47e40b1d7f21192dbb1e71af83090bc4c888a71c6eee3f03` | `fa3f17a819f30a1cc108b74a1d50787b86b619ac852f09e753704b6d15a59226` |
| 18 dB | `73903dfe863b24b395c5712a4ce42e7e4fb76f8054c12fcc9f493f1447c7d2bf` | `5b4076ca7a3c7dc2548f9c3dbbe18aa79d23f4f2ae3192d07e4634c9a746faa0` | `a9bf87153d281432c0670148ad0b779f00722d18336cdf2214bb313aea586896` |

The individual source member paths are locked as `Impaired_C2C_{10,16,18}dB_P1_to_P2/*_{THRU,FEXT,NEXT}_ExtPEC.s4p` with exact original spellings in `SOURCES`. The physical roles are `thru`, `fext`, `next`; original port order `[1,2,3,4]` is also guarded in the runner. Membership is distinct across the **three historical channel labels**; two package tests of one label reuse the same historical source group, not six distinct measured channels.

**Critical inventory limitation:** although each source group includes a real NEXT .s4p member, the **actually evaluated 012_13 fixture used 4 instances of a single FEXT original and 0 NEXT aggressors**. This is neither four physically independent FEXT measurements nor proof that zero NEXT is the complete, permitted final PHY topology. This alone does not establish a false source: it establishes an **unqualified use/selection** requiring explicit same-profile justification.

## 4. Six Test1/Test2 **candidate** receipts — not six qualified final Goldens

The six stored finite COM/FOM pairs are prior SiCoPR Python versus Octave-compatible 4.17 *candidate* computations, with verified same effective CSV/source identity. They do not make the draft model normative.

| Channel | Test | TX/RX package (mm) | Candidate COM Python/Octave (dB) | Candidate FOM Python/Octave (dB) | Final reference eligibility |
| --- | ---: | --- | --- | --- | --- |
| 10 dB | 1 | 13 / 11 | 4.8425407043560735 / 4.84254070435603 | 17.359998002530823 / 17.35999800253053 | **BLOCKED** |
| 10 dB | 2 | 31 / 29 | 4.697201007609777 / 4.697201007609771 | 17.244092226424666 / 17.244092226424662 | **BLOCKED** |
| 16 dB | 1 | 13 / 11 | 4.293845073865506 / 4.293845073865389 | 16.825974111052055 / 16.825974111051995 | **BLOCKED** |
| 16 dB | 2 | 31 / 29 | 3.9994128151173114 / 3.999412815117313 | 16.532274640085806 / 16.532274640085788 | **BLOCKED** |
| 18 dB | 1 | 13 / 11 | 4.054649183385672 / 4.05464918338567 | 16.560322876677272 / 16.560322876677603 | **BLOCKED** |
| 18 dB | 2 | 31 / 29 | 3.5828402112060016 / 3.5828402112060003 | 16.108377606388284 / 16.10837760638834 | **BLOCKED** |

Prior numerical correlation was within `0.0001 dB` for each candidate COM and FOM, with maximum observed absolute deviations approximately `1.17e-13 dB` and `3.30e-13 dB`. That is **within the shared-version candidate correlation contract only**, and is not a transferable 012_44 tolerance or normative PHY proof.

## 5. Official reference, reproduction and implementation independence

| Category | Recorded source/evidence | Eligibility |
| --- | --- | --- |
| Official open-source reference | `802-com/com_code` **4.17.0**, commit `47459695a169fdf1fd10c541a5b19c69a5ae9285`; file `release/com_ieee8023_4p17p0.m` SHA-256 `cdcd739ac13c9cc3ca59733fd938a015f040429655b5dbf5543c897789cfed9b` | **READY for source pinning only**, not direct MATLAB numeric execution |
| Third-party compatible implementations | `tbermen/SiCoPR` commit `cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75`, Python + Octave-compatible 4.17 implementation | Reproduction of **historical candidate** documented |
| Real historical numerical CI | [#44070019965](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965) — source/negative tests, 3-channel × 2-package candidate Python/Octave runs | Historical candidate **PASS** only |
| Original IEEE MATLAB runtime | The official original `com_ieee8023_4p17p0.m` was **not directly executed** in the 012_13 correlation | **UNVERIFIED** for final reference execution |
| Numerical independence | Python and Octave are separately executed, but share one pinned SiCoPR project and compatible 4.17 algorithm lineage | **PARTIAL / not completely independent oracle** |
| Native SParamView C++ COM | Production `calculateCom()` remains `NotEvaluated`; no qualified final optimized C++ COM result | **BLOCKED**; 012_37–45 |
| Official version choice | 4.17.0 retained unless an explicit 012_31 official-release/compatibility assessment changes the frozen commit | Newer official release and compatibility **UNVERIFIED here**, never automatically substitute |

Reproduction paths: [candidate script](../scripts/com_120f8_reference_candidate.py) with `--channel 10|16|18`, `--mode python|octave`, pinned `--external`, and the [comparator](../scripts/com_120f8_reference_compare.py). These run in ephemeral CI only, with source hashes and effective CSV identity checked. The [workflow](../.github/workflows/com-120f8-real-numerical-reference.yml) is path-filtered and **does not automatically rerun its full six-case numerical qualification on this documentation-only PR**. Offline parser tests alone are not equivalent to those real reference runs.

## 6. License and distribution review

| Item | Recorded provenance | Allowed claim / restriction |
| --- | --- | --- |
| Licensed IEEE 802.3ck-2022 original | Prior 012_11 private licensed PDF SHA and page audit | **No source PDF, pages, account metadata or licensed excerpts committed or uploaded** |
| Official IEEE COM 4.17 source | Root and file BSD-3-Clause reported in 012_02. Root LICENSE SHA-256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631` | Attribution/conditions apply; source is kept in ephemeral reference environment |
| SiCoPR third-party port | 012_02 records BSD-3-Clause and exact source commit | Do not silently bundle; respect repository's license/notice requirements |
| Public IEEE 2019 C2C ZIP and 2022 workbook | Public URLs plus prior pinned byte hashes; publication as tool input is **not by itself a blanket redistribution license** | Reuse external URLs and SHA receipts; redistribution permission **UNVERIFIED**, do not commit raw files |
| Golden numerical receipts | Original source-free numbers/provenance in committed JSON | Numerical reproduction allowed as existing audit metadata; **not** PHY compliance certification |

For 012_42–45, any required privately licensed source must be accessed in a lawful isolated environment; raw data and third-party source must not leak to public Git, logs, artifacts or releases.

## 7. Explicit gate matrix and blockers

`READY` below means evidence for **that one subclaim**, never global IEEE COM readiness.

| Gate | Result | Specific evidence or missing condition |
| --- | --- | --- |
| SOURCE | **READY (historical only)** | 2019 IEEE ZIP + nine pinned S4P SHA, prior CI execution |
| PROFILE | **BLOCKED** | No proof 2020 aggressor package overlay and the historical 2022 workbook/PHY fixture equal the final approved 120F test |
| CHANNEL | **BLOCKED** | 4 repetitions of one FEXT source and 0 NEXT used despite available distinct NEXT file; normative aggressor selection/count not approved |
| PORT | **PARTIAL** | Historical port order/orientation guarded; both-case package + complete reference-plane equivalence still requires 012_33 |
| PACKAGE | **BLOCKED** | Final TX/RX numeric values READY, but NEXT/FEXT adapter geometry and propagation-delay assumptions from historical draft unqualified |
| VOLTAGE | **UNVERIFIED** | Entire final profile voltage/normalization/reference-plane link not yet independently established |
| STATISTICS | **UNVERIFIED** | Final 93A DER/noise/jitter/ISI combination and ownership not yet qualified |
| REFERENCE | **PARTIAL** | Official 4.17 source pinned; original MATLAB not run and final approved same-profile input run absent |
| REPRODUCIBILITY | **READY for candidate / BLOCKED for final** | Prior CI historical six cases only; no approved final-input rerun |
| INDEPENDENCE | **PARTIAL** | Python/Octave separate executions share compatible implementation lineage |
| LICENSE | **PARTIAL** | COM/SiCoPR code license documented; raw channel/workbook redistribution rights not established |

**Required to lift REFERENCE BLOCKED (not automatically the 012_44 numerical PASS):**

1. Locate and pin the **final approved same-PHY** aggressor/package/channel configuration and its normative basis (or prove the exact current topology is valid); record authoritative clause/figure/source, effective config hashes, and every selected NEXT/FEXT role, count and port direction.
2. Execute/reproduce a final-profile reference under an auditable pinned official implementation (or document a reviewed equivalent independent method); preserve permitted source-free provenance. A shared SiCoPR lineage alone cannot constitute strong independence.
3. Establish full Test1 and Test2 input reference-plane/package/voltage/statistics roles, with a valid per-metric tolerance methodology **fixed before** future C++ comparison.
4. Confirm needed source/data licenses and permitted handling. Preserve non-redistribution where uncertain.
5. Ensure 012_31's equation/function Gap Matrix and official reference version-lock decision are **actually evidenced**, not inferred from roadmap approval.

Do not manufacture final-PHY READY from the candidate values, an unverified license, an unavailable reference implementation or an exploratory `Warning/NOT_IEEE_COM` interval.

## 8. What may proceed and what must stay blocked

- **012_33–41:** May advance with source-verified, bounded fixed-input **subengine** proofs while explicitly retaining missing final reference evidence as a known dependency. 012_33 must lock both Test1/Test2 complex-response paths and their reference planes.
- **012_42–44:** The final same-PHY end-to-end numerical PASS cannot occur until the above REFERENCE BLOCKED items are resolved. 012_44 additionally requires six independent actual same-profile C++ versus external reference comparisons with prelocked per-metric limits.
- **012_45/46, v1.4.0 release:** Production IEEE COM(dB) PASS/FAIL and its GUI certification claims remain disabled. Preserve `calculateCom() == NotEvaluated` for unqualified input and retain current release/tag assets.

## 9. Execution and completion evidence checklist

This report is a source-traceable **audit decision**, not a claim to have executed the final numeric solver. The implementation does not change. Separate the following evidence types:

- **Previously executed:** [012_11 source audit](COM_8023ck_Normative_Audit_012_11.md), [012_13 numerical CI](https://github.com/july-rust/SParamView-Public/actions/runs/44070019965), 012_13 strict negative offline tests, prior SHA source checks.
- **Examined for this audit:** source/config/runner, committed candidate JSON and existing reports. The current audit's final reference status is **BLOCKED** as detailed above.
- **Not executed/verified in 012_32:** licensed PDF rehash, fresh 2019 ZIP download, official MATLAB reference runtime, six final-PHY computations, native C++ IEEE COM result, cross-OS native GUI execution or formal PHY certification.
- **PR gate:** capture latest PR HEAD SHA, actual required CI and CodeQL status, substantive review, squash merge SHA and post-merge integration CI. If those are missing, record `012_32 PARTIAL — audit written, completion gate open`; **never** claim 100% COMPLETE.

**Acceptance terminology:** `012_32 COMPLETE (audit) — REFERENCE BLOCKED` is valid *only after* the PR/CI/merge gates are satisfied; it means the evidence gap was precisely identified, **not** that the reference inputs or COM engine are qualified.
