# 012_28 — Receiver Cursor / Bounded Joint-Tail Exploratory Ratio (NOT IEEE COM)

**KST:** 2026-11-23. **Target:** `integration/v1.4.0`.
**Contract:** a bounded C++20 bridge, independent CTest, and one development plan entry. No normative COM, GUI, main, release or tag change.

## What this actually computes

The 012_21/012_22 validated receiver-equivalent victim pulse gives the signed cursor `C` at index 3 (UI 0). Under the explicit synthetic PAM4 model with levels `{-1,-1/3,+1/3,+1}`, the nominal decision half-spacing is `S=abs(C)/3` volts. The existing 012_25/012_26 combined victim-ISI + NEXT/FEXT engine returns the finite-precision **upper** one-sided tail quantile `N_upper` in volts; six selected signed taps are enumerated, and omitted taps are included through the absolute-amplitude envelope `B`. The diagnostic is

```text
R_exploratory_lower_dB = 20 * log10(S / N_upper)
R_two_case_min_dB = min(R_TEST1, R_TEST2)
```

This is **not** IEEE 802.3ck COM(dB), target DER, FOM, margin, normative Test1/Test2 decision or PHY PASS/FAIL. The algorithm cannot establish real physical independence, validated TX/RX volts calibration or a statistically certified numerical error enclosure. The mixture bound is marked `NUMERICALLY_BRACKETED_NOT_CERTIFIED`; the word "lower" applies only to the assumed mixture and estimated finite-precision noise upper bound, not to a certified physical BER/DER or a rigorous floating-point lower limit.

## Implementation and source isolation

- New `ComCursorTailRatioCasesData` through `calculateComCursorTailRatioBridge()`.
- Calls `calculateComCombinedTailBridge(raw, expectedPair, live)` **again**, and thus replays 012_22 victim source validation, 012_24 aggressor/live identity checks, 012_25 selected-and-omitted tap construction, and 012_26 numerical qualifications. Never accepts an unverified cached summary.
- Signal taken from the revalidated `ComScaledVoltageCase.cursorEquivalentV`; it cannot be supplied independently by the caller.
- Enforces same Test identity, receiver decision plane, voltage calibration, nonzero finite normal cursor/half-spacing, positive normal noise bound, properly ordered selected interval, allowed numerical/bound modes, and matching one-sided probability.
- Reuses the pure, provenance-free numeric kernel `calculateDiagnosticVoltageRatioDb()` exposed from the already-tested 012_19 ratio module. **Do not fabricate or rewrite provenance case tokens:** 012_19 requires delimiters `|case=TEST1|`, while 012_25 case-specific settings end with `case=TEST1`. Existing callers of 012_19 retain their original provenance handling.
- Each usable output remains `Warning/NOT_IEEE_COM`, `012_28-1` algorithm provenance; `calculateCom()` remains `NotEvaluated`.

## Qt-free qualification

New CTest `channel_com_cursor_tail_ratio` uses deterministic 012_25-style synthetic source fixtures but independent mathematical Gaussian q reference `0.08529781587845649257 V` at one-sided `p=1e-5`, sigma `0.02 V`. It asserts independently computed `20*log10((.16/3)/q)` and `20*log10((.20/3)/q)` to `1e-4 dB`; the existing 012_26 numerical bracket has a bounded finite width. It checks victim-only, victim+NEXT, victim+NEXT+FEXT, eight-tap omitted-bound cases; two-Test isolation, common voltage scaling invariance, probability/model contract, source identity and calibration fail-closed, nonfinite noise, invalid cursor, unsupported probability, undeclared independence, cancellation, and that official `calculateCom` stays `NotEvaluated`.

Pure numeric amplitude validation remains positive/finiteness-checked and uses the same near-unity direct ratio / extreme safe-log fallback as 012_19. No runtime Python, GUI or FFT dependency.

## Completion gates

Required: latest PR HEAD Linux Release & ASan/UBSan, Windows x64/ARM64 and native ARM64 package checks where applicable, macOS arm64, CodeQL, relevant CTests (012_19–012_27), substantive independent review when available, protected squash-merge to `integration/v1.4.0`, post-merge integration CI. When evidence is missing, report **PARTIAL**, never 100% COMPLETE. IEEE Std 802.3ck-2022 is a specification target, not a qualification conferred by this synthetic diagnostic.

**Repository provenance:** `include/si/channel_analysis/com_voltage_ratio.hpp`, `src/channel_analysis/com_voltage_ratio.cpp`, `src/channel_analysis/com_victim_isi.cpp`, `src/channel_analysis/com_combined_tail.cpp`, `docs/COM_Full_Mixture_High_Precision_012_27.md`.
