# 012_02 — COM Profile Lock (evidence-based decision)

**Observed 2026-11-15 KST** · **Status: BLOCKED — NOT LOCKED** · Official SParamView baseline `v1.3.5`, target `v1.4.0`.

## Evidence vs qualification

| Subject | Verified factual finding | Unverified / required for lock |
|---|---|---|
| Normative definition | IEEE Std 802.3 and amendments specify COM by Annex **93A** and **178A** equations/methods; the software is an **example implementation**, not the normative text | Exact parameter table / clause for selected PHY |
| **Approved candidate** | **IEEE Std 802.3ck-2022**, 100GAUI-1 C2C, Annex 120F; use Annex 93A method only where published IEEE PHY explicitly incorporates it | Final normative COM table/algorithm configuration, full parameter values and 2 test cases remain UNVERIFIED; candidate not a supported profile |
| Official code | **COM v4.17.0**, Git tag `4.17.0`, commit `47459695a169fdf1fd10c541a5b19c69a5ae9285` (GitLab API verified) | Compatibility with target PHY parameter table not yet tested |
| Newer activity | 2026-10-20 ad hoc shows proposed COM 4p17 requests; meeting listings do not prove a newer released code | Confirm actual release history and commit tree |
| Repository | GitHub Actions fetched IEEE GitLab API, source tag, root LICENSE and release file metadata (200 OK); root BSD-3-Clause LICENSE SHA-256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631` | Channel fixtures and execution remain unavailable; see [CI 43691394632](https://github.com/july-rust/SParamView-Public/actions/runs/43691394632) |

IEEE official evidence:
- [COM project](https://www.ieee802.org/3/ad_hoc/COM/index.html)
- [COM release 4.17.0 announcement, 2026-09-25](https://ieee802.org/3/ad_hoc/COM/email/msg00126.html)
- [COM ad hoc 2026 contributions](https://www.ieee802.org/3/ad_hoc/COM/public/telecon/index.html)
- [IEEE SA Ethernet catalog](https://standards.ieee.org/ieee/802.3/10422/)

## 2026-11-15 follow-up: why 012_02 remains blocked

[CI 43693613982](https://github.com/july-rust/SParamView-Public/actions/runs/43693613982) interrogated GitLab **tag 4.17.0**, not speculative page names.

- `config_templates/C2C/200G` contains spreadsheet names `config_com-4p11p0_802p3dj_d2p1_200G_C2C_...`, `...d2p2...` and `...d2p3...`.
- `config_templates/C2M/200G` also contains `802p3dj_d2p2` and `802p3dj_d2p3` templates.
- KR/CPC templates are organized by 200G and 400G directories, not a separately identified 802.3ck-2022 100GAUI-1 C2C approved parameter sheet.
- **IEEE P802.3dj was still in the standards approval pipeline in November 2026**, so draft D2.x configuration spreadsheets are not a substitute for a published 802.3ck-2022 normative COM profile. Sources: [IEEE P802.3dj Oct 2026 conditional RevCom submission](https://www.ieee802.org/3/email_dialog/msg01881.html), [approved 802.3ck-2022 catalog](https://standards.ieee.org/ieee/802.3ch/7322/).
- IEEE source function `com_ieee8023_(varargin)` and BSD 3-Clause notice verified in actual `release/com_ieee8023_4p17p0.m`; this **does not** establish it can reproduce approved 802.3ck settings without a selected valid configuration.
- Until a published 802.3ck source-compliant configuration + independent COM numerical oracle are actually executed, do not mark `LOCKED`, PASS/FAIL or release-ready.

## Lock contract — all fields mandatory

`tests/fixtures/channel/com_reference/manifest.json` is the machine-readable contract. The candidate `profile_id` deliberately ends in **CANDIDATE** and `status=BLOCKED`. Do not show this as a supported profile in UI.

To lock: record (1) immutable IEEE edition/amendment, normative annex, PHY clause and its exact parameter table/COM threshold, plus `approval_status=PUBLISHED` and published IEEE normative source URL; (2) **pin verified:** v4.17.0 tag + `47459695...` source commit; (3) TX/RX EQ, ISI, jitter, noise, package, THRU/NEXT/FEXT file grouping and port/direction; (4) reference execution with independent expected values and fixtures; (5) fixture/dependency license evidence; (6) explicit numerical tolerance rationale.

Selection principle: prefer a stable released IEEE PHY profile with obtainable official configuration and examples. ANNEX alone is **not** a complete profile; do not impose a generic COM=3 dB criterion on arbitrary channels.

**Decision:** keep `COM = NOT_EVALUATED`; 012_02 **cannot be 100% COMPLETE** based on these documents. Core/UI work that is profile-independent may continue, but release remains gated.
