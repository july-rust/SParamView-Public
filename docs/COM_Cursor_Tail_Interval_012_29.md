# 012_29 — Exploratory Cursor/Tail Voltage-Ratio dB Interval (NOT IEEE COM)

**KST:** 2026-11-23. **Base:** `integration/v1.4.0`. **Predecessor:** [012_28](COM_Cursor_Tail_Ratio_012_28.md), PR #92.

## Scope

This change extends the **already replayed** 012_28 receiver-equivalent voltage cursor and the 012_25/26 selected+omitted PAM4 tail bounds. No new statistical solver, new UI, release, official COM or extra upstream replay is introduced.

For each Test, let `S=abs(cursorEquivalentV)/3 > 0` (a declared synthetic PAM4 half-spacing), `N_u=upperTailBoundV > 0`, and `N_l=lowerTailBoundV` (possibly zero or negative after conservative omitted-tap subtraction). Use the validated, provenance-free 012_19 safe logarithmic voltage-ratio kernel:

```text
exploratoryLowerRatioDb = 20*log10(S / N_u)
exploratoryUpperRatioDb = 20*log10(S / N_l)  iff N_l > 0
exploratoryUpperRatioDb = nullopt             iff N_l <= 0
```

`noiseLowerTailBoundV` exposes the original signed lower voltage bound for diagnosis; it is **not** clamped to zero or promoted to a valid noise amplitude. A missing upper endpoint is not represented by `0`, `NaN`, `+Inf`, or `DBL_MAX`. Requiring a positive upper noise bound preserves the existing fail-closed lower endpoint. A real upper endpoint must be finite and not below the lower endpoint.

For the two-Test minimum: `min(L1,L2)` is the original lower endpoint. If both finite upper endpoints are available, its upper is `min(U1,U2)`; if only one exists, the known `Ui` is still an upper bound on `min(R1,R2)` because `min(R1,R2) <= Ri <= Ui`. If neither exists, the pair upper is missing.

## Trust boundary and provenance

This is a **model-conditional exploratory interval**. Gaussian independent receiver noise, independent uniform PAM4 aggressors, independent voltage scaling, and the receiver decision plane are mathematical input contracts, not independently calibrated physical-channel evidence. `NUMERICALLY_BRACKETED_NOT_CERTIFIED` remains in force: outward omitted-tap magnitude arithmetic and finite precision probability endpoint checks do not prove directionally rounded `erfc` evaluations or a rigorous real-number enclosure. Thus even a finite two-ended interval is **not** IEEE Std 802.3ck Annex 93A COM(dB), target DER, FOM, certified BER/DER margin, or PHY PASS/FAIL.

Input is always revalidated by a **single** `calculateComCombinedTailBridge(input,expectedPair,live)` replay per request, including current SourceIdentity, package Test, signed victim pulse, all NEXT/FEXT calibration, noise ownership, selected six taps, omitted-tap envelope, numerical qualification and cancellation. No caller-provided cached combined-tail summary is accepted. The 012_28 signal source and existing ratio lower outputs are unchanged; the output provenance advances to `012_29-1` solely to identify the expanded interval contract.

All successful outputs remain `Status::Warning` + `NOT_IEEE_COM`. The production `calculateCom()` remains `Status::NotEvaluated`.

## Independent qualification

Extend the **existing** Qt-free `channel_com_cursor_tail_ratio` CTest. Do not add CMake targets or runtime Python. The fixture's independent high-precision Gaussian-only `sigma=0.02 V`, `p=1e-5`, `q=0.08529781587845649257 V` validates both endpoints against analytically calculated `20*log10((0.16/3)/q)` and `20*log10((0.20/3)/q)` within `1e-4 dB`. Also test:

- Test1/Test2 minimum upper-endpoint algebra, non-inverted intervals and legacy lower output.
- Consistent common-voltage scaling of cursor and Gaussian noise for **both** endpoints, and signed-negative cursor symmetry.
- 20 independently identified synthetic NEXT/FEXT aggressors with conservatively large omitted envelopes: first only Test1's lower noise bound becomes nonpositive, then both do. Confirm optional one-sided behavior and the surviving Test2 minimum upper bound.
- Independent long-double reference for the 012_19 pure safe-log kernel at extreme finite positive voltages, plus near-unity ratio.
- Inherited invalid/stale source, wrong cases/planes, nonfinite Gaussian, zero cursor, calibration mismatch, undeclared independence, unsupported probability, and pre-cancellation.
- The official `calculateCom()` returns `NotEvaluated`.

## Completion

Latest PR HEAD mandatory Linux Release, sanitizers, Windows x64/ARM64 and package where applicable, macOS arm64, CodeQL, COM regression, substantive review when available, protected squash merge to `integration/v1.4.0`, and post-merge CI are required. Any missing required evidence is PARTIAL; do not claim normative IEEE COM certification. Official v1.3.5 tags/assets and `main` remain unchanged.
