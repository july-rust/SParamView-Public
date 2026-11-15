# 012_02 — COM Profile Lock (evidence-based decision)

**Observed 2026-11-15 KST** · **Status: BLOCKED — NOT LOCKED** · Official SParamView baseline `v1.3.5`, target `v1.4.0`.

## Evidence vs qualification

| Subject | Verified factual finding | Unverified / required for lock |
|---|---|---|
| Normative definition | IEEE Std 802.3 and amendments specify COM by Annex **93A** and **178A** equations/methods; the software is an **example implementation**, not the normative text | Exact parameter table / clause for selected PHY |
| Candidate standard | **IEEE Std 802.3-2022, Annex 93A** is the provisional analysis baseline | Edition-specific chosen PHY clause, table, all settings, limit and standard text verification |
| Official code | IEEE 802.3 chair announced **COM v4.17.0 on 2026-09-25**, after September Working Group approval | Exact `com_code` commit SHA/tag and compatibility with chosen PHY |
| Newer activity | 2026-10-20 ad hoc shows proposed COM 4p17 requests; meeting listings do not prove a newer released code | Confirm actual release history and commit tree |
| Repository | `https://opensource.ieee.org/802-com/com_code` announced as official | GitLab content inaccessible in this execution environment (web HTTP 418; container DNS failure) |

IEEE official evidence:
- [COM project](https://www.ieee802.org/3/ad_hoc/COM/index.html)
- [COM release 4.17.0 announcement, 2026-09-25](https://ieee802.org/3/ad_hoc/COM/email/msg00126.html)
- [COM ad hoc 2026 contributions](https://www.ieee802.org/3/ad_hoc/COM/public/telecon/index.html)
- [IEEE SA Ethernet catalog](https://standards.ieee.org/ieee/802.3/10422/)

## Lock contract — all fields mandatory

`tests/fixtures/channel/com_reference/manifest.json` is the machine-readable contract. The candidate `profile_id` deliberately ends in **CANDIDATE** and `status=BLOCKED`. Do not show this as a supported profile in UI.

To lock: record (1) immutable IEEE edition/amendment, normative annex, PHY clause and its exact parameter table/COM threshold; (2) pinned release + 40-hex source commit; (3) TX/RX EQ, ISI, jitter, noise, package, THRU/NEXT/FEXT file grouping and port/direction; (4) reference execution with independent expected values and fixtures; (5) source/fixture license evidence; (6) explicit numerical tolerance rationale.

Selection principle: prefer a stable released IEEE PHY profile with obtainable official configuration and examples. ANNEX alone is **not** a complete profile; do not impose a generic COM=3 dB criterion on arbitrary channels.

**Decision:** keep `COM = NOT_EVALUATED`; 012_02 **cannot be 100% COMPLETE** based on these documents. Core/UI work that is profile-independent may continue, but release remains gated.
