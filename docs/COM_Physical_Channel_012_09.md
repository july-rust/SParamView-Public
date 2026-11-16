# 012_09 — Physical 4-port and balanced package numerical core

**State:** IMPLEMENTED PHYSICAL SUBSYSTEM, not qualified IEEE 802.3ck COM, not 012_09 COMPLETE.

## Actual code and acceptance

1. `s_network.hpp/cpp` — Full single-ended 4x4 complex S matrix (pNear,nNear,pFar,nFar); exact unitary mixed-mode transform; differential Sdd11/Sdd21/Sdd12/Sdd22 with equal single-ended Zref -> 2*Zref differential, explicit maximum mode-conversion. Rejects a full nonpassive physical matrix using complex Cholesky of I-S^H*S, mismatched port references, excessive mode conversion, malformed/active inputs.
2. `cascadeComScattering` — Passive, matched Zref Redheffer star product retaining all internal reflections. No lossy ABCD determinant subtraction, including high-loss networks.
3. `package.hpp/cpp` — Fully parameterized balanced dual-conductor die-to-board package: differential equivalents of shunt Cd/Cb/Cp, series Ls, two independent impedance/length transmission-line segments with explicit frequency-dependent loss and propagation delay. Reverses element order for board-to-receiver die; *does not hardcode unreviewed IEEE final coefficients*.
4. `s_network_tests.cpp` — Identity, analytic series/shunt ABCD equivalence, passive 200 Np cascade, exact complex phase, ideal and unbalanced four-port mixed-mode, differential impedance normalization, active four-port, inconsistent references, degenerate internal reflections.
5. `com_package_tests.cpp` — Differential capacitance/inductance and matched complex transmission analytical goldens, frequency-dependent alpha/sqrt(f) model, line lengths, reversed two-port S11/S22 identity, negative/nonfinite parameter rejection.

## Critical remaining standard-compliant COM work

This is a **real electromagnetic network/circuit computation**, but IEEE COM requires much more: approved complete final 802.3ck-2022 Table 120F-8 values, precise reference 93A.1 package propagation law and terminations, PAM4 statistical impulse/noise/ISI/XTALK/DFE/CTLE/TX-FFE optimization and the final numeric COM ratio, plus >=3 independently reproduced same-802.3ck-PHY channel goldens tested with **both** package tests. The existing `calculateCom()` still returns NotEvaluated/Unsupported, not a numerical COM value. No UI activation, PHY PASS, or v1.4.0 release.

Use the source SHA / release and QA-only third-party policy in `tests/fixtures/channel/com_reference/manifest.json`. Current `status=REFERENCE_READY`, `cases=[]` must not be upgraded without real independent receipts. This component makes it possible to later connect actual S4P channels to physical package models, but 4-port transformation alone does not supply the missing IEEE COM procedure.

## Sources and evidence limitations

- [IEEE Std 802.3ck-2022 approved standard/catalog](https://standards.ieee.org/ieee/802.3ck/7322/)
- [IEEE official COM code](https://opensource.ieee.org/802-com/com_code)
- [IEEE task-force historical COM/package tool series](https://www.ieee802.org/3/ck/public/tools/)
- [IEEE public 2020 COM-parameter comparison (historical draft, not final norm)](https://www.ieee802.org/3/ck/public/adhoc/sept23_20/wu_3ck_adhoc_01a_092320.pdf)
- [Independent third-party COM config implementation reference (Ansys 2025 R1, not normative)](https://ansyshelp.ansys.com/public/Views/Secured/Electronics/v251/en/Subsystems/Circuit/Content/Circuit/COMAnalysisSpec8023CK_100GAUI_1_C2C_200GAUI_2_400GAUI_4_C2C.htm)

## Reproduction

Run `cmake -S . -B build-com -DSI_BUILD_GUI=OFF`, `cmake --build build-com --target si_com_s_network_tests si_com_package_tests`, and `ctest --test-dir build-com -R channel_com_ --output-on-failure`. Any claim of numeric/physical PASS must use actual executed CI and the exact current commit SHA.
