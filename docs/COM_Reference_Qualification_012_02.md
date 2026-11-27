# 012_02 — COM reference evidence and reproducible numeric oracle

**Phase evidence: COMPLETE (REFERENCE_READY); official PHY conformance: NOT EVALUATED.**

## 1. Pinned official COM source

- IEEE open source GitLab `802-com/com_code` tag **4.17.0** → commit `47459695a169fdf1fd10c541a5b19c69a5ae9285`.
- `release/com_ieee8023_4p17p0.m`, 533,634 bytes, content SHA-256 `cdcd739ac13c9cc3ca59733fd938a015f040429655b5dbf5543c897789cfed9b`.
- Source and root LICENSE have **BSD-3-Clause** notice. License SHA-256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631`.
- Actual GitLab/IEEE archive inspection evidence: [CI #43691394632](https://github.com/july-rust/SParamView-Public/actions/runs/43691394632), [#43693613982](https://github.com/july-rust/SParamView-Public/actions/runs/43693613982), [#43695378748](https://github.com/july-rust/SParamView-Public/actions/runs/43695378748).
- Historical IEEE 802.3ck COM 3.70 package ZIP SHA `0accfa982bbb85d3ee4b368c889d1e1126a91662dce3e7eac18ab75591351bbb`, legacy MATLAB source SHA `b756baf4fbd73ac1d9917e104b8cffba33eb6c1de9b47d5627c2d1a8f6424ac9`, 120F C2C Excel candidate SHA `85b861404f98009981c22e02ac216e264bdb057bb017db89323ef2c971517173`. Execution in Octave **NOT VERIFIED** (MATLAB-version guard adapted, then `cell2mat` dimension mismatch); no false PASS.

## 2. Actual two-condition COM 4.17 cross-correlation

**Real execution:** [GitHub Actions #43696202018](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018), [text evidence artifact](https://github.com/july-rust/SParamView-Public/actions/runs/43696202018/artifacts/14154413427). This is **SiCoPR** independent Python COM v4.17 port recomputing values from original IEEE publicly hosted channel files, and comparing against the prior published **Octave** COM 4.17 reference outputs. It does **not** execute Microsoft's MATLAB license, nor directly execute IEEE 4.17 MATLAB source during this job.

- Independent port commit: `cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75`, project [SiCoPR](https://github.com/tbermen/SiCoPR), BSD-3-Clause.
- Public source: [IEEE P802.3dj channel ZIP](https://www.ieee802.org/3/dj/public/tools/CR/akinwale_3dj_02_2311.zip), download ZIP SHA-256 `8a3da48ac684293946f5690493e02584dbe3223ec92cc5033417727512621e95`.
- Input verified by *six individual SHA-256 hashes* in [CHANNEL.md](https://github.com/tbermen/SiCoPR/blob/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75/examples/akinwale_CR_22dB_VendorX/CHANNEL.md); one THRU, three FEXT, two NEXT.
- Independent expected source: [EXPECTED.md](https://github.com/tbermen/SiCoPR/blob/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75/examples/akinwale_CR_22dB_VendorX/EXPECTED.md), published COM 4.17 Octave reference results.

| Condition | Published Octave COM (dB) | New actual Python COM (dB; CI printed) | FOM (published → new) | Status |
|---|---:|---:|---|---|
| THRU only | 3.5070028402502937 | 3.50700284025 | 12.697866613999423 → 12.697866614 | PASS |
| THRU + FEXT(3) + NEXT(2) | 2.895898684873241 | 2.89589868487 | 12.094255868167135 → 12.0942558682 | PASS |

COM, FOM, itick=-6, ERL, VEC, VEO, ICN passed the independent project's specific numerical tolerances; COM and FOM ±`1e-9` dB. Exact differences shown above compare rounded CI display values; double-precision estimates from published document are **not** misrepresented as our new unrounded run output. CI output ends with `all pinned values reproduced` (exit 0).

**Limit:** channel/config is IEEE **P802.3dj draft**, *not* the approved IEEE 802.3ck-2022 Annex 120F package Test 1/Test 2 conditions; successful algorithm correlation is not 802.3ck conformance. Final approved PHY Table 120F-8 settings and ≥3 independent **same-profile normative** cases remain mandatory in 012_09/11.

## 3. Reproduction and audit

- `scripts/com_417_numeric_reference_ci.py` clones independent repository at exact commit in GitHub Actions, downloads third-party IEEE channel package to a temporary directory, validates six SHA-256, calculates two real cases and compares published expected metrics. Raw source and channel data are **not** committed, uploaded, or packaged.
- `scripts/com_reference_preflight.py --mode reference-ready` checks readiness metadata and **cannot** independently replace the actual execution gate. Both numerical job and metadata gate must pass.
- `scripts/com_reference_preflight.py --mode qualify` is the original strict **normative** gate and intentionally fails at 012_02. Later 012_09/11 must supply approved standard full numeric parameters, Test 1/Test 2, ≥3 real fixtures and their independent reference results, licensed evidence and tolerances.

**No SParamView COM C++ engine, Qt window or v1.4.0 release was implemented in 012_02.**
