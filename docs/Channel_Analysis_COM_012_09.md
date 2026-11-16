# 012_09 COM Numerical Prequalification — BLOCKED (not IEEE COM)

**Status:** PARTIAL IMPLEMENTATION / 012_09 NOT COMPLETE. This is an auditable engineering handoff, not a COM-margin qualification.

## 2026-11-16 incremental implementation and independent evidence

- **PR #49** prequalification fail-closed core, [initial CI #43822495908](https://github.com/july-rust/SParamView-Public/actions/runs/43822495908), merged `077dfd92bf206c435c358612d9ca302c5c1496c2`.
- **PR #50** independent reciprocal complex ABCD/two-port numerical tests, [CI #43827019566](https://github.com/july-rust/SParamView-Public/actions/runs/43827019566), merged `925d25cc2d9ed1d519e8873e1d9fd6c19f72e6df`.
- **PR #51** passive 4-port S matrix -> differential mixed mode, Redheffer multiple-reflection cascade, physically explicit TX/RX package circuits, [CI #43832394254](https://github.com/july-rust/SParamView-Public/actions/runs/43832394254), merged `099d4ca34bf1801e01d9d1243f4fc988d0f7686f`; [post-merge CI #43835229303](https://github.com/july-rust/SParamView-Public/actions/runs/43835229303) SUCCESS.
- **PR #52** official IEEE-hosted historical 2022 3ck SA C2C workbook **read-only** SHA evidence, exact [audit CI #43835564179](https://github.com/july-rust/SParamView-Public/actions/runs/43835564179) and [three-platform CI #43835564311](https://github.com/july-rust/SParamView-Public/actions/runs/43835564311) SUCCESS; merged `ed6d086e526eda7a2521ca302cac75f1e7e192ae`, post-merge [audit #43837671707](https://github.com/july-rust/SParamView-Public/actions/runs/43837671707) and [three-platform #43837671808](https://github.com/july-rust/SParamView-Public/actions/runs/43837671808) SUCCESS. Archive SHA `895857fbfb602c8ec10b221ab40a23d4b39244b4b57101dd27abc4b2a1a2fddc`, workbook SHA `e676b3fb3cb3048f80c98deaa8faca1d03c13daa216c6259de26885e715ca925`; **27 candidate named fields** with adjacent literal numeric values, NOT qualified final normative IEEE Table 120F-8. No third-party workbook redistributed. See [audit details](COM_2022_SA_Reference_Audit_012_09.md).
- **PR #53** deterministic finite-state PAM4/Gaussian tail quantile, cancellation, target-relative probability convergence, nanovolt/femtovolt scale adversarial tests, [exact-head CI #43837614094](https://github.com/july-rust/SParamView-Public/actions/runs/43837614094) SUCCESS: Linux release CTest **41/41**, ASan/UBSan **41/41**, Windows x64/ARM64 plus native ARM64 package and macOS arm64. Merged `cb6448d8672a3c391ef07c14f15290c91e63176b`; [post-merge CI #43845851063](https://github.com/july-rust/SParamView-Public/actions/runs/43845851063) must be checked independently. See [statistics limitations](COM_Statistics_012_09.md).

- **PR #54** auditable 012 development plan and numerical status reconciliation, merged `f01bc11372f931ef48678f7c623383f412ff1acc`; [post-merge CI #43849332236](https://github.com/july-rust/SParamView-Public/actions/runs/43849332236) **SUCCESS**.
- **PR #55** complete diagnostic pipeline from **real SHA-pinned Touchstone S4P** (all 16 physical Sij, confirmed four-port mapping) through phase-preserving TX/PCB/RX physical package cascade for distinct Test1/Test2. Independent S4P/ABCD analytic and actual Cache/SourceSnapshot/Touchtone tests. [PR CI #43854149149](https://github.com/july-rust/SParamView-Public/actions/runs/43854149149) **SUCCESS**: Linux Core Release **42/42** and ASan/UBSan **42/42**, Windows x64/ARM64 plus native ARM64 package, macOS arm64. Merged `16288759ae453d45b3da01ec1741a5d9f3f5ea83`. [Post-merge CI #43857020290](https://github.com/july-rust/SParamView-Public/actions/runs/43857020290) needs independent final-status verification. The [fully documented circuit API](COM_Physical_Test_Cases_012_09.md) is **not** the IEEE COM dB engine.

All changes above are confined to `integration/v1.4.0` and are **non-releasing**. IEEE COM dB remains unavailable until published final 802.3ck-2022 Table 120F-8 is fully and independently verified, IEEE-compliant receiver/jitter/ISI/XTALK/CTLE/DFE/FFE optimization is integrated, both Test1/Test2 are evaluated, and **at least three distinct same-PHY independent goldens** agree for both package cases. The strict qualification gate stays fail-closed; `012_09 != 100% COMPLETE`.

## 012_09 latest physically grounded S-parameter network expansion

The independent [physical four-port, Redheffer and balanced package API](COM_Physical_Channel_012_09.md)
adds exact complex mixed-mode extraction with complete physical port
reference/passivity checks; passive S-parameter cascade with multiple internal
reflections; and explicitly parameterized TX/RX package die-to-board networks.
This is physical circuit computation, NOT the still-absent standard COM dB.

## Additional independently tested two-port physics

See [012_09 complex two-port/ABCD verification](COM_Two_Port_Numerics_012_09.md).
Implemented passivity-gated series/shunt/line/network cascade and both
scattering directions with independent analytic identities. The final
802.3ck C2C nominal Test1 package lengths (TX/RX 13/11 mm) and Test2
(31/29 mm) are distinct typed selectors, **not** certified full package COM.

## Implemented

- Independent Qt-free C++20 com.hpp/com.cpp with matched-wave, phase-preserving spectral preparation for explicit TX FIR FFE, complex RX filter, THRU and distinct NEXT/FEXT aggressors.
- Validates identical measured frequency grids (including DC), differential port modes, matching reference impedances, finite taps/complex samples, Nyquist coverage, aggressor identity and cancellation.
- COM margin entrypoint calculateCom returns NotEvaluated with **no numeric value**. No synthetic 3 dB or false IEEE PASS/FAIL can escape.
- Core CTest si_com_tests covers analytic TX FIR, complex FEXT phase, crosstalk, invalid source/profile/band/impedance and cancellation.
- Production CLI qualifier now uses qualify_normative to reject candidate/draft PHY, missing final-table evidence, lack of three distinct channel cases, and incomplete package Test1/Test2 evidence. The old qualify helper remains for synthetic checker unit tests.

## Critical limitations

| Requirement | Status |
|---|---|
| Final IEEE 802.3ck-2022 Table 120F-8 numeric audit | BLOCKED: no approved checked parameter artifact |
| Physical S4P input, real Touchstone SHA/mapping source, two loaded TX/PCB/RX package cases and complete reflection-preserving two-port cascade | **IMPLEMENTED / integrated diagnostic**, not connected to a qualified IEEE COM solver |
| Independent finite-state PAM4/Gaussian tail quantile | **IMPLEMENTED** as exploratory statistics kernel; NOT IEEE 93A COM PDF |
| IEEE-specific pulse/ISI/noise/jitter model and EQ optimization | **NOT IMPLEMENTED** as validated normative chain |
| Normative COM numerator/denominator and dB | NOT IMPLEMENTED |
| Both Test1/Test2 C2C package results | NOT EVALUATED |
| 3 independent same-PHY 802.3ck goldens | NOT AVAILABLE |
| C++ implementation vs independent normative reference | NOT EVALUATED |

Matched-wave scalar conditioning is useful numerical scaffolding but is **not** a standards-accurate package/network simulator. Its numbers must never be labeled IEEE COM. The 012_02 two-condition reference uses P802.3dj draft-era channel inputs, not final 802.3ck normative tests.

## Reproduction

1. cmake -S . -B build-com -DSI_BUILD_GUI=OFF
2. cmake --build build-com --target si_com_tests
3. ctest --test-dir build-com -R channel_com_ --output-on-failure
4. python scripts/com_reference_preflight.py --mode reference-ready --manifest tests/fixtures/channel/com_reference/manifest.json --root tests/fixtures/channel/com_reference
5. python scripts/com_reference_preflight.py --mode qualify --manifest tests/fixtures/channel/com_reference/manifest.json --root tests/fixtures/channel/com_reference

The strict production qualifier MUST return exit code 2 while normative evidence is absent. Record real workflow run and SHA in the PR. No COM GUI activation, no 012_09 COMPLETE claim, and no v1.4.0 release.

## Primary evidence

- [IEEE approved 802.3ck-2022 standard](https://standards.ieee.org/ieee/802.3ck/7322/)
- [IEEE COM open source](https://opensource.ieee.org/802-com/com_code)
- [012_02 independently reproduced non-normative numerical reference](COM_Reference_Qualification_012_02.md)
- [012 series acceptance plan](Development_Plan_012_v1.4.0.md)
