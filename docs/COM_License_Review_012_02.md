# 012_02 — COM licensing and redistribution decision

**REFERENCE_READY (QA-only external source strategy); release redistribution audit still required at 012_13.**

| Resource | Actual source evidence | In SParamView build/release? |
|---|---|---|
| Official [IEEE 802-COM v4.17.0](https://opensource.ieee.org/802-com/com_code) | Repo LICENSE + source header **BSD-3-Clause**; license SHA-256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631` | **No**; externally pulled for QA only |
| Independent [SiCoPR](https://github.com/tbermen/SiCoPR) Python port | `cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75` has its own BSD-3-Clause LICENSE and NOTICE | **No**; ephemeral CI dependency only |
| Public IEEE P802.3dj channel data | [IEEE channel archive](https://www.ieee802.org/3/dj/public/tools/CR/akinwale_3dj_02_2311.zip); six files verified SHA-256 in CI | **No**; downloaded temporarily, no claims of redistribution rights |
| IEEE Std 802.3ck text and COM settings | [Published standard](https://standards.ieee.org/ieee/802.3ck/7322/) (copyrighted) | **No** text reproduced; normative numeric parameters must be audited under proper access |
| Python/Octave/MATLAB runtime | Third-party transitive licenses vary; no proprietary MATLAB runtime redistributed | **No**; CI only |

**Choice:** implement a standalone C++20 COM engine in 012_09, use externally pinned official reference/SiCoPR **only for QA correlation**, keep SParamView source MIT and do not embed official source, proprietary runtime or third-party channel files in a release. The chosen no-redistribution *architecture* is confirmed. This is not a blanket grant to copy protected IEEE text or external datasets.

Release gates that remain explicitly at 012_13: all actual packaged dependencies and notices, patent/attribution obligations if applicable, licensed numeric parameter reference, source provenance and dataset access. Do not conflate technical BSD3 metadata with a legal opinion.

Evidence: [IEEE official-source inspection](https://github.com/july-rust/SParamView-Public/actions/runs/43691394632), [COM algorithm correlation](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018), [SiCoPR BSD3 license](https://github.com/tbermen/SiCoPR/blob/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75/LICENSE).
