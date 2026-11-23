# 012_33 — Test1/Test2 complex physical-input contract

**Date:** 2026-11-24 KST  
**Target:** `integration/v1.4.0`  
**Scope:** bounded, source-anchored **historical causal** S4P → differential → TX/board/RX packages → fixed FFE/CTLE/receiver complex transfer.  
**Qualification level:** **NOT_IEEE_COM / REFERENCE_BLOCKED**. This work does **not** calculate COM(dB), FOM, detector error ratio, full PHY conformance or a standards PASS/FAIL.

## 1. Predecessor evidence and hard limits

- [012_31 reviewed plan](Development_Plan_012_v1.4.0.md): the execution slot 012_33 is physical Test1/Test2 contract and independent fixed complex response only. Approval of the roadmap must **not** be mistaken for an independently complete IEEE equation/function Gap Matrix.
- [012_32 reference eligibility audit](COM_120F_Reference_Eligibility_012_32.md): **REFERENCE BLOCKED** remains. Historical six-case Python/Octave values used draft package overlays, four repeated FEXT inputs and zero NEXT. They are not final 802.3ck goldens.
- [012_11 approved numeric table audit](COM_8023ck_Normative_Audit_012_11.md): 68 published numeric field values are source-traced, but that is not qualification of the full package propagation and fixed-receiver algorithm.
- Source-anchored causal producer: `calculateHistoricalCausalC2cFromSource()`; historically different attenuation-only producer: `calculateComPhysicalFromSource()`. They expose different model identifiers intentionally.

**Important model mismatch analysis:** `prepareCom120F8LoadedTestCases()` permits only `EXPLORATORY_HISTORICAL_8023CK_CAUSAL_PACKAGE_S4P`. The older `EXPLORATORY_S4P_TX_BOARD_RX_PHYSICAL_CASCADE` implements a different attenuation-only package and MUST remain rejected. String relabeling would conceal the difference between dispersive phase/loss and attenuation-only propagation. 012_33 adds a source-backed typed entrypoint that constructs the causal model; it does not weaken the older guard.

## 2. Implemented boundary

`prepareCom120F8HistoricalCausalFromSource(source, victimChannelId, fixedEq, observedLiveIdentity, maxModeConversion, control)`:

1. Rejects a source whose captured identity differs from the caller's supplied observed workspace identity **before** expensive work.
2. Validates fixed 120F-8 FFE and CTLE ranges and the mode-conversion bound.
3. Extracts the actual sixteen physical complex Sij via the confirmed `SourceSnapshot` port mapping `[nearP,nearN,farP,farN]`.
4. Preserves 50 Ω single-ended physical port waves; converts to 100 Ω differential waves, preserving forward/reverse complex responses and reflections. Mixed-mode conversion above the declared limit, non-passive data and frequency/reference inconsistency remain failures.
5. Invokes **the existing historical causal** package producer, with Test1 TX/RX lengths 13/11 mm and Test2 TX/RX lengths 31/29 mm, respectively; secondary line length 1.8 mm in each. TX path is die→board, RX path board→die.
6. Reuses the pre-existing fixed-EQ preparation chain: cursor-relative FFE, complex CTLE, receiver filter and one Gaussian TX transition response. The current code's receiver-filter transfer is a **diagnostic matched-wave component**, not a proven complete normative signal/noise ownership model.
7. Rechecks the caller-supplied observed identity, preserves measured SHA-256/revision/mapping/workspace/mapping generations and adds a deterministic, locale-independent hexadecimal-floating-point settings identity. The settings identity now also includes the **exact selected victim ID as length-prefixed hexadecimal UTF-8 bytes**, because the mapping identity describes the *whole* source mapping, not one chosen victim; this closes the Codex P2 cross-victim collision finding. The source owner must still recheck **actual live** state before publishing results; a caller-provided identity cannot detect an external change never passed to this function.
8. Returns `Warning` with `NOT_IEEE_COM` and `REFERENCE_BLOCKED`; never enables `calculateCom()` or a PHY verdict.

The new entrypoint is deliberately a **historical causal experimental bridge**: provenance names, diagnostic messages and model identity do not promote this to final IEEE COM.

## 3. Independent complex numerical reference

**Source:** a deterministic synthetic passive 4-port, not a borrowed IEEE channel or six-case final PHY corpus. The board is balanced, matched at 50 Ω per physical port, with differential transmission

\[
S_{dd21}^{board}(f)=S_{dd12}^{board}(f)=0.75\,e^{-j2\pi f\,12\mathrm{ps}},
\quad S_{dd11}^{board}=S_{dd22}^{board}=0.
\]

The grid is DC to **26.57 GHz** in **10 MHz** increments (2,658 samples), meeting the existing strict direct-grid requirement. Each of the 16 Sij entries is written into a temporary Touchstone S4P fixture at test runtime; no external standards PDF, licensed model, channel archive or reference implementation is redistributed.

For numerical independence, a separate Python scalar 2×2 **ABCD matrix** calculation (not C++ `cascadeComScattering`, `calculateComCausalPackage` or an application-generated Golden) was used to precompute the expected complex values. The oracle multiplies, in physical order, single-ended shunt capacitance/series inductance, both complex transmission lines with dispersive \(\gamma(f)\), matched-board ABCD and reverse RX package. It converts total ABCD to S with Zdiff=100 Ω. In the RX stage it multiplies the **cursor-centered** fixed FFE, explicit CTLE, receiver filter and 7.5 ps Gaussian transition factor exactly once.

The exact fixed EQ is `c[-3,-2,-1,0,+1]=[-0.04,0.10,-0.20,0.56,-0.10]`, `gDC=-20 dB`, `gDC2=-4 dB`. These are **test inputs**, not optimizer-selected final coefficients.

### Pre-locked numerical Golden

At 10 GHz, 100 Ω differential reference:

| Quantity | Test1 real | Test1 imag | Test2 real | Test2 imag |
| --- | ---: | ---: | ---: | ---: |
| Loaded S11 | -0.013062413672278447 | -0.19048964173558278 | -0.1549611649491547 | -0.15094647200903374 |
| Loaded S21 | 0.5916511922056075 | 0.19225745881316827 | 0.28504798985641333 | -0.40432116587475825 |
| Loaded S22 | 0.030514690993256547 | -0.12145878853157195 | -0.0922806182801023 | -0.16783622490938535 |
| Fixed EQ/RX conditioned H | 0.09903983926314527 | 0.041418880560227654 | 0.054732367610984996 | -0.06551197911112154 |

At 26.57 GHz, conditioned H:

| Test | Real | Imag |
| --- | ---: | ---: |
| Test1 | -0.1987064153710583 | -0.11649354516659073 |
| Test2 | 0.0019288695483848457 | -0.1507199568823488 |

At DC both cases have physical S21=0.75 and conditioned H=0.01514297626752464 (real). The computed results intentionally retain dramatically different Test1/Test2 phase at higher frequency.

**Tolerance locked before CI:** absolute `5e-10` in each **real** and **imaginary** dimension of the dimensionless complex response. This is a component-level floating-point regression tolerance, **not** an IEEE COM(dB) or PHY signoff tolerance. No post-run fitting or looser dB-only matching is permitted.

The offline independent values are pinned in `tests/channel_analysis/com_physical_contract_012_33_tests.cpp`. This file contains **no read-back of C++-computed Golden fixtures** and does not run an external runtime during CI. The C++ CTest compares both constructed in-memory S4P and actual parser/cache/SourceSnapshot paths.

## 4. Fail-closed regression

The dedicated Qt-free CTest `channel_com_012_33_physical_contract` checks:

- Full complex S11/S21/S22, reciprocal S12, fixed FFE/RX conditioning and separate Test1/Test2 roles;
- Wrong Test2 selector and RX length;
- Attempted attenuation-only model relabeling and incorrect 50 Ω differential reference;
- Missing S4P sample, unequal physical-port Zref and active/non-passive Sij;
- Off-grid CTLE, unmapped victim channel and nonfinite mode-conversion limit;
- Mismatched workspace generation returning `Status::Stale` without a result, and cancellation propagation;
- Source hash/revision/mapping generation and deterministic settings identity;
- **Two-victim S8P regression**: one source/mapping/generation containing two distinct 4-port victims with distinct complex transmission; confirms the shared global source identity but different victim-specific settings identities and deterministic repeat selection.
- Production `calculateCom()` still `NotEvaluated`.

**External-mapping limitation:** This test detects inconsistent or absent mapping but cannot independently prove a human-chosen four-port direction matches the true connector pinout; that requires source-specific physical evidence. An intentional differential sign reversal cannot be diagnosed by merely observing valid S4P.

## 5. Verification and completion gates

- Build/test with `-DSI_BUILD_GUI=OFF` for the bounded numerical CTest; retain the existing CTest fleet without altering release workflows.
- **Native ARM64 gap correction:** the existing `Windows ARM64` job on the general three-platform workflow performs an x64-hosted cross-build and PE/deployment audit; this is **not** native CTest execution. A single additive, path-scoped, Qt-free workflow (`.github/workflows/com-01233-native-arm64.yml`) uses the official `windows-11-vs2026-arm` GitHub-hosted runner to build/run this one numeric CTest on actual ARM64. This does not weaken any existing required checks, release rule or source-qualification gate. Both cross-build and native runs must pass.
- Review code changes on final PR HEAD, including the identity check scope, the model distinction and the independent ABCD oracle provenance.
- Required GitHub CI (Linux, Windows x64, native Windows ARM64, macOS arm64) and CodeQL need evidence before declaring a completed merged task.
- Squash merge to `integration/v1.4.0` only through existing protection. Verify post-merge integration CI.
- Keep existing `main`, v1.3.5 tag/assets and COM GUI untouched.

**Permitted terminal status on success:** `012_33 COMPLETE — PHYSICAL INPUT CONTRACT QUALIFIED (BOUNDED)`, while simultaneously retaining `FINAL PHY REFERENCE BLOCKED` and `IEEE COM(dB) NOT EVALUATED`.

**Handoff to 012_34:** take the verified fixed complex response's explicit Test1/Test2 phase, timing and package identity as a starting point for cursor/ISI/DFE qualification. Do not claim that this physical bridge establishes official 93A detector error or the unqualified aggressor topology.
