# 012_19 — COM voltage-ratio diagnostic and independent mathematical verification

**Implemented scope:** A Qt-free C++20 mathematical voltage ratio; **NOT** the IEEE 802.3ck-2022 Annex 93A COM engine, FOM optimizer, target-DER statistical solver, PHY PASS/FAIL or a COM(dB) value.

## Numerical contract

The diagnostic computes 20 log10(A_signal/A_noise) from strictly positive, finite **volt** amplitudes. Signal and noise must refer to the same named receiver decision plane and calibration. It uses direct log10 of a representable ratio and log subtraction when intermediate division would overflow or underflow. It accepts separate Test1 and Test2 cases and gives the smaller *diagnostic* ratio, never a normative worst-case COM claim.

Inputs are source-tagged Result<ComVoltageEvidence> values with matching 64-hex SHA-256, revision, mapping identity, workspace/mapping generations, exact case-specific settings, and nonempty upstream algorithm/version. Non-usable results, missing values, mixed case/receiver planes/calibrations, invalid voltages and cancellation fail closed. An arbitrary caller-provided provenance object cannot cryptographically certify the original data, so none of these checks acts as a normative qualification token.

Every successful result is Status::Warning with NOT_IEEE_COM and an EXPLORATORY model string. Production calculateCom() remains NotEvaluated with no margin and no verdict. No GUI, main branch, previous tag or release is modified.

## Why this is not full IEEE COM

012_14 crosstalk pulses are dimensionless diagnostics; 012_15 Gaussian RX noise and 012_16 RJ/Dual-Dirac are local models; 012_17/012_18 calculate bounded ideal DFE and local ISI optimization, not normative COM FOM/BER/DER; 012_09 eight-tap PAM4 mixture is not full Annex 93A. The 012_13 six-case COM reference is a historical engineering candidate with explicit 2020 aggressor assumptions. These quantities are not silently combined into official A_signal/A_noise.

## Golden and acceptance

The dedicated channel_com_voltage_ratio CTest compares 2:1 = 6.020599913279624 dB, 1:1 = 0 dB, 1:2 = -6.020599913279624 dB and common voltage scaling. Extreme positive doubles, case isolation, invalid source/mapping/settings, plane/calibration mismatch, missing input, stale state, cancellation and unchanged production COM gate have negative tests. This qualifies only the mathematical utility and its contract.

Implementation review, exact-HEAD CI and squash-merge evidence must be recorded in PR; unexecuted checks are not PASS.

Sources: [IEEE Std 802.3ck-2022](https://ieeexplore.ieee.org/document/9999414), [012_18 local search](COM_Equalizer_Search_012_18.md), [012_13 independent reference candidate](COM_120F8_Reference_Numerical_012_13.md).
