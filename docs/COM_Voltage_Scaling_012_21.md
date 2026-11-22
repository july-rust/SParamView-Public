# 012_21 — COM receiver-equivalent voltage scaling (NOT IEEE COM)

## Boundary and actual implementation
A bounded, Qt-free C++20 scale conversion ties the existing \`Result<Com120F8RectangularPulseCases>\` to **two separate, caller-declared** \`Result<ComVoltageScaleEvidence>\` instances. It preserves the signed -3..+9 UI unit-symbol pulses and calculates one receiver-*equivalent* voltage array for each Test1/Test2 case.

The calculation is **only** \`V_equivalent[k] = voltsPerUnitSymbol * h_unitSymbol[k]\`. This is an explicitly defined mathematical scaling, **not** measured Vout/Vin, not a physical receiver termination or source impedance conversion, and not an IEEE Annex 93A-compliant voltage calibration. In particular the pinned \`victimPeakV\` in Table 120F-8 is **not silently inserted** as the multiplier and Sdd21 power-wave transfer is **not presumed** to equal physical voltage gain. A caller's claimed calibration cannot be cryptographically verified by matching text identifiers.

## Provenance and constraints
The upstream aggregate must have the currently implemented paired-pulse algorithm/version \`com-120f8-two-package-symbol-pulse-exploratory\` / \`012_09_03-pulse-1\`. The expected channel and all three \`Result\` metadata must agree on channel SHA-256, revision, confirmed mapping, settings, workspace and mapping generations, and must carry nonempty upstream algorithm/version. The paired result uses **shared pair settings**, not a one-case token; each scale evidence explicitly identifies TEST1 or TEST2.

Both calibrations must agree on transmitter reference plane, receiver decision plane, normalization contract, calibration identity and TX/RX equalizer identities. **Different external calibration-source identities are permitted**, as those may refer to separate independently sourced evidence; each one must be nonempty. No independent physical authenticity is implied.

Only \`EXPLORATORY_VOLTS_PER_SIGNED_UNIT_SYMBOL\` normalization is admitted. A strictly positive, finite, normal volts-per-unit-symbol scale is required. Each pulse must have 13 finite samples at -3..+9 UI, valid symbol rate, ordered times, a consistent maximum, and the same Test1/Test2 time grid. Nonfinite, overflow/underflow, missing, stale, mismatched or cancelled inputs fail closed. Runtime is fixed two passes of 13 multiplications, without FFT, integration or a new optimizer.

All successful results are \`Status::Warning\` with \`NOT_IEEE_COM\` and model \`EXPLORATORY_RECEIVER_EQUIVALENT_UNIT_SYMBOL_VOLTAGE\`. No formal voltage calibration, COM FOM/DER, standalone statistical dependence, PHY PASS/FAIL, Joint Tail autowiring or \`calculateCom()\` activation follows from this change.

## Qualification and evidence
The dedicated \`channel_com_voltage_scaling\` CTest uses independent exact arithmetic Goldens: Test1 scale 0.2 V, pulse precursor/cursor/postcursor -0.1/0.8/0.2 → -0.02/0.16/0.04 V; Test2 scale 0.4 V, pulse 0.25/0.5/-0.2 → 0.10/0.20/-0.08 V. It tests scale proportionality, unchanged source arrays, case isolation, stale/malformed identity, missing calibration, nonfinite/overflow, invalid time grid and cancellation. It verifies the unchanged production \`calculateCom()\` NotEvaluated gate.

The test values are **synthetic mathematical cases**, not final IEEE 802.3ck channel Goldens. Only actual exact-HEAD CI and review receipts can establish completion. No final 802.3ck COM criterion is asserted.

## Sources
- [IEEE Std 802.3ck-2022 approved publication](https://standards.ieee.org/ieee/802.3ck/7322/)
- [012_20 bounded joint tail](COM_Joint_Tail_012_20.md)
- [012_19 diagnostic voltage ratio](COM_Voltage_Ratio_012_19.md)
- [012_09 symbol-pulse numerical primitive](../include/si/channel_analysis/com_120f8.hpp)
