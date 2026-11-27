# 012_02 — COM profile identity and reference readiness

> **2026-11-17 012_11 최종 갱신:** 최종 IEEE Std 802.3ck-2022 전체 승인 원문(PDF 316쪽, SHA-256은 [감사 기록](COM_8023ck_Normative_Audit_012_11.md) 참조)과 Table 120F-8의 구현 수치 68개를 직접 대조하여 **012_11 규격값 감사 COMPLETE (68/68)**. 아래 2026-11-15의 미래 일정 표현은 당시 계획 기록이다. 독립 Golden 및 C++ COM PASS/FAIL의 **NOT EVALUATED** 상태는 변경하지 않았다.

**2026-11-15 (KST) · 012_02: COMPLETE / REFERENCE_READY; IEEE production PHY conformance: NOT EVALUATED.**

## Approved PHY identity (the only v1.4.0 target)

| Property | Locked identity | Verification boundary |
|---|---|---|
| Official standard | [IEEE Std 802.3ck-2022](https://standards.ieee.org/ieee/802.3ck/7322/) (approved 2022-09-21) | Published, not draft |
| Scope | 100GAUI-1, **C2C** (chip-to-chip) | Not generic IEEE, not MIPI |
| Normative reference | Annex **120F.4.1**, COM procedure **93A.1**, parameter **Table 120F-8** | **Complete normative parameter values and the reference configuration verification remain at 012_09** |
| Criterion | COM ≥ 3 dB **for each Test 1 and Test 2** package case | Must evaluate both for real 802.3ck PASS/FAIL |
| IEEE source engine | COM **4.17.0**, tag commit `47459695a169fdf1fd10c541a5b19c69a5ae9285`, SHA-256 `cdcd739ac13c9cc3ca59733fd938a015f040429655b5dbf5543c897789cfed9b` | Source availability, not engine conformance |
| Independent algorithm oracle | SiCoPR commit `cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75`, against published Octave 4.17 results | **2 real non-normative P802.3dj-channel correlations PASS; not 802.3ck qualification** |

## Deliberate separation of phase gates

- **012_02 = REFERENCE_READY:** approved PHY identity, IEEE reference source and license pinned, independently computed COM v4.17 **non-normative algorithm** correlation on THRU-only and THRU+FEXT/NEXT inputs, numeric tolerance, immutable input SHA, no third-party code/data redistributed in SParamView. [Actual run #43696202018](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018)
- **012_09 = PROFILE_AND_ENGINE_QUALIFIED (pending):** verify all numeric parameter values in the *final* IEEE Std 802.3ck-2022 Table 120F-8 (not draft xlsx); run both Test 1 and Test 2; obtain ≥3 independent, valid **same-PHY** reference goldens; implement C++ engine and test with no unresolved mismatches.
- **012_11 = NUMERIC_CROSS_VALIDATED (pending):** independent oracle, existing RL/IL/NEXT/FEXT/TDR and all baseline regressions.
- **012_13/14 = RELEASE_GATE (pending):** third-party dependency/fixture rights, packaging/notice checks, Windows/macOS native tests, full CI and SHA-256. No premature v1.4.0 tag.

**Important:** IEEE 802-COM tag v4.17.0 `config_templates/C2C/200G` examples explicitly reference `P802.3dj D2.x` *drafts*. Those spreadsheets are **never** treated as an approved 802.3ck profile. The 2022 COM 3.70 archive contains a legacy 120F C2C candidate but uses pre-ratification `3ck D3.1` settings and its Octave execution **failed at workbook parsing**; it must not certify the 2022 final specification.

Machine contract: [COM manifest](../tests/fixtures/channel/com_reference/manifest.json). `status=REFERENCE_READY`, `cases=[]` (normative tests **still absent**). The strict `--mode qualify` must fail until actual 012_09/11 test evidence replaces this readiness gate.

Sources: [IEEE 802.3ck-2022 official listing](https://standards.ieee.org/ieee/802.3ck/7322/), [IEEE 802.3ck working group](https://www.ieee802.org/3/ck/), [IEEE COM official source](https://opensource.ieee.org/802-com/com_code), [IEEE 802.3ck COM historical tools](https://www.ieee802.org/3/ck/public/tools/).
