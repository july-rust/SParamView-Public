# 012_02 — COM license and distribution qualification

**Status: NOT CLEARED — fail closed for source/data reuse** (2026-11-15).

## Findings and evidence limits

| Component | Evidence | Distribution decision |
|---|---|---|
| IEEE COM source | IEEE SA COM open-source project approved and repository publicly announced | **NOT CLEARED** until actual repo LICENSE and all relevant files checked |
| Preliminary license | March 2025 IEEE SA Open tutorial states project chose **BSD-3 clause license agreement** | Preliminary evidence only; not proof that every current file, table, spreadsheet and dependency is under BSD-3-Clause |
| IEEE standard text | Annex 93A / 178A equations and methods are normative; software reference is distinct | **Do not copy** copyrighted standard text into this repository; cite edition/clause and implement within lawful access |
| Fixture S-parameters | IEEE presentation links do not grant unrestricted data redistribution | **NOT CLEARED** pending fixture author/license and hash |
| MATLAB runtime | May require separate proprietary runtime/license | No MATLAB redistribution authorized by this review |
| Octave/Python ports | Independent code, transitive dependencies and licenses must be read per actual artifact | **NOT CLEARED**; never infer same terms as IEEE reference |
| Windows/macOS binaries | No third-party COM code, spreadsheets or unlicensed datasets included by this stage | Existing v1.3.5 assets unaffected |

Sources:
- [IEEE COM project](https://www.ieee802.org/3/ad_hoc/COM/)
- [IEEE COM repository availability announcement](https://www.ieee802.org/3/ad_hoc/COM/email/msg00013.html)
- [IEEE SA Open tutorial, March 2025, slide 15](https://mentor.ieee.org/802-ec/dcn/25/ec-25-0088-00-LMSC-2025-mar-tutorial-slides-open-source.pdf)
- [IEEE SA OSCom operations manual](https://standards.ieee.org/wp-content/uploads/import/documents/other/OSCOM_Operations_Manual.pdf)

## Preferred architecture (conditional)

**Option B — independent C++20 implementation + official reference only in licensed QA environment** is preferred for portability (Windows x64 / native ARM64 / macOS arm64), test isolation and long-term maintainability. This is an engineering preference, **not** a legal conclusion that it may reproduce every element of a standard without access/permission.

No official `*.m`, `*.xlsx`, S-parameter third-party datasets or third-party license text were copied to SParamView by this change.

## Mandatory pre-release evidence

1. Retrieve canonical GitLab commit SHA, LICENSE file contents and SHA-256; verify SPDX ID per included file and third-party deps.
2. Verify copyright/notice reproduction, source modification obligations, patent/attribution requirements, data rights and platform runtime licenses.
3. Verify official dataset redistribution or use external download at test time with rights and checksum; otherwise create original synthetic fixtures and independent reference in licensed environment.
4. Inventory per Windows/macOS release package; obtain needed author permission when license is ambiguous. If ambiguous, **block redistribution**.

This technical screening is not a substitute for formal legal advice; report licensing as **unverified** until the actual artifact and terms are examined.
