# 012_25 — Combined Victim Self-ISI + NEXT/FEXT Bounded Statistical Tail (NOT IEEE COM)

**Status:** Implementation and independent CTest submitted; qualification is conditional on latest PR HEAD CI, substantive review, protected squash merge, and post-merge checks. No PHY COM(dB) or formal PASS/FAIL.

## Scope

Revalidates **raw inputs** through 012_24 (which reruns 012_22 victim pulse validation and checks independently passed current SourceIdentity), then combines 012_21 signed victim noncursor self-ISI and 012_24 signed NEXT/FEXT per-aggressor receiver-equivalent pulses into one 012_20 statistical mixture **per Test1/Test2**.

- Victim UI 0 **is a signal cursor**, excluded from noise; its other 12 UI offsets are candidate self-ISI taps.
- An aggressor's UI 0 **is interference**, included with its other 12 samples.
- Up to 32 aggressors with 13 samples each plus 12 victim ISI samples = 428 candidate taps per case. Zero-valued taps do not create states.
- Sort by descending absolute signed voltage, then role (VICTIM, NEXT, FEXT), aggressor ID and UI offset for deterministic ties.
- Select at most 6 nonzero taps **across all sources**. Reuse the one 012_20 engine: at most 4^6 = 4096 independent PAM4 states (8192 including nonzero Dual-Dirac).
- All omitted taps remain recorded by role, ID, UI and original polarity. Their absolute amplitudes are summed with outward-rounded additions to form **B**. A shifted-tail envelope is [q_selected - B, q_selected + B], also outward-rounded. There is no quantile-to-quantile addition.
- q_selected is from 012_20 and carries its finite numerical solver tolerance. The reported envelope is a mathematical bound conditional on exact q_selected under the assumed model; it is **not a rigorously certified floating-point enclosure**.
- No new FFT, Monte Carlo, Gaussian distribution or PHY implementation.

## Physical meaning and caveats

**Assumption:** each tapped PAM4 symbol is uniformly and independently distributed among {-1, -1/3, +1/3, +1}. The caller must explicitly select this model. This is an exploratory assumption **not proof of physical decorrelation**, neither between aggressors nor across conditioned transfers.

The input includes Test-specific receiver Gaussian, RJ Gaussian and Dual-Dirac noise. Only the original three noise components are fed into the 012_20 solver once per Test, avoiding duplicate RX noise. All noises and voltage taps must share the same RX decision plane and calibration identity. Each aggressor retains its own TX plane/EQ/calibration-source identity: different aggressors need not have the victim TX EQ.

**Critical upstream limitation:** 012_24 calibration evidence is caller-declared; the 012_14 prepared interference inputs do not carry an independently auditable per-aggressor physical TX conditioning lineage. Provenance and equalizer strings by themselves are not proof of the underlying PHY calibration. Therefore every successful result is \`Warning / NOT_IEEE_COM\`. Nothing in this PR enables \`calculateCom()\`, DER, FOM, BER or PHY qualification.

## Fail-closed checks

- Reinvoke 012_24 from original \`ComAggressorVoltageInput\` and CURRENT live \`SourceIdentity\`, not a cached output summary.
- Enforce the original 012_21/012_22/012_24 source SHA, revision, mapping, settings, generations, exact algorithm/version, 13 UI alignment, signed pulse/peak integrity, independent per-Test gains and calibration provenance.
- Enforce Test1/Test2 isolation, common RX plane/calibration, case-qualified RX/RJ/dual-Dirac provenance, common RX EQ, victim TX EQ matched to its noise; allow legitimately distinct aggressor TX EQ.
- Reject nonfinite/subnormal voltages, excessive aggressor count, missing assumption, unqualified probability, duplicate noise ownership or IDs, numerical overflow, stale live identity and cancellation.
- Output worst-case exploratory upper tail = max(Test1 upper bound, Test2 upper bound), **not** the IEEE-defined worst-case COM metric.

## Independent mathematical qualification

Synthetic independent SciPy/long-double state-enumeration examples, one-sided p=1e-5, receiver Gaussian sigma=0.02 V, no RJ or Dual-Dirac unless specifically specified:

| Case | Signed receiver-voltage taps (V) | Independent one-sided q (V) |
| --- | --- | ---: |
| RX Gaussian only | none | 0.0852978158784565 |
| Victim self-ISI only | -0.02, +0.04 | 0.1323069531212122 |
| Victim + NEXT | -0.02, +0.04, -0.10, +0.20 | 0.416686608710545 |
| Victim + NEXT + FEXT | -0.02, +0.04, -0.10, +0.20, +0.18, -0.12 | 0.6967286340771246 |

Tests cover 0/1/2/3/32 aggressors, NEXT/FEXT alone and jointly, UI 0 aggressor interference, Test1/Test2 case isolation, seven-plus taps, independent 8-tap full enumeration, max 8192 Dual-Dirac states, deterministic permutation, stale source and calibration, invalid noise and cancellation. The eight-tap case checks that a separately computed full quantile lies within the reported bounded interval.

The numeric reference examples are *mathematical synthetic tests*, not external PHY reference channels.

## Files and acceptance

- \`include/si/channel_analysis/com_combined_tail.hpp\`
- \`src/channel_analysis/com_combined_tail.cpp\`
- \`tests/channel_analysis/com_combined_tail_tests.cpp\`
- \`CMakeLists.txt\`: \`channel_com_combined_tail\`
- This single note and one new entry in \`Development_Plan_012_v1.4.0.md\`

Required completion evidence: newest PR HEAD Linux Release + ASan/UBSan, Windows x64/ARM64/native package, macOS arm64, CodeQL, existing 012_20–012_24 tests, substantive Codex review if available, squash-merge to \`integration/v1.4.0\` according to protection requirements, and post-merge checks. If any mandatory evidence is missing, report PARTIAL; never claim a completed release. Do not edit \`main\`, v1.3.5 tags or release assets.

Related: [012_20](COM_Joint_Tail_012_20.md), [012_21](COM_Voltage_Scaling_012_21.md), [012_22](COM_Victim_ISI_012_22.md), [012_23](COM_Victim_ISI_Tail_012_23.md), [012_24](COM_Aggressor_Voltage_012_24.md).
