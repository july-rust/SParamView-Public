# 012_02 — COM license and distribution qualification

**Status: BSD-3-Clause confirmed for root IEEE COM repository v4.17.0; dependency/fixture redistribution remains NOT CLEARED** (2026-11-15).

## Findings and evidence limits

| Component | Evidence | Distribution decision |
|---|---|---|
| IEEE COM source | Official v4.17.0 GitLab tag `47459695...`; root LICENSE and README verified in GitHub Actions | Root code indicates **BSD 3-Clause**; individual copyright notices and dependencies still require inventory |
| License evidence | At tag `4.17.0`, the actual 1516-byte `LICENSE` identifies **BSD 3-Clause**; SHA-256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631`; README confirms BSD3 | Verified repository-level license; not automatic permission for externally sourced tables/fixtures |
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

1. **DONE for root reference:** GitLab tag `4.17.0` = commit `47459695a169fdf1fd10c541a5b19c69a5ae9285`; root LICENSE BSD-3-Clause SHA-256 verified. Still inspect copyright/SPDX for each included source and dependency.
2. Verify copyright/notice reproduction, source modification obligations, patent/attribution requirements, data rights and platform runtime licenses.
3. Verify official dataset redistribution or use external download at test time with rights and checksum; otherwise create original synthetic fixtures and independent reference in licensed environment.
4. Inventory per Windows/macOS release package; obtain needed author permission when license is ambiguous. If ambiguous, **block redistribution**.

Repository root BSD 3-Clause is **verified** at v4.17.0 using [GitHub Actions 43691394632](https://github.com/july-rust/SParamView-Public/actions/runs/43691394632). Separate COM example files, external datasets and runtime dependencies remain **unverified** until individually inspected. Technical screening does not replace formal legal advice.
