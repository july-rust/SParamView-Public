# 012_09 COM Numerical Prequalification — BLOCKED (not IEEE COM)

**Status:** PARTIAL IMPLEMENTATION / 012_09 NOT COMPLETE. This is an auditable engineering handoff, not a COM-margin qualification.

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
| Full package network including reflection, 2-port cascade | NOT IMPLEMENTED |
| IEEE-specific pulse/ISI/noise and EQ optimization | NOT IMPLEMENTED |
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
