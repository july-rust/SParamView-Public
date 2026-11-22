# 012_22 — Receiver-equivalent victim SELF-ISI voltage statistics

## Scope and provenance

This **Qt-free, bounded, non-normative** C++20 helper analyzes only the two
already-scaled -3..+9 UI signed symbol responses produced by 012_21.
The caller passes `Result<ComScaledVoltageCases>` and the expected **current**
source identity. Each case has 13 samples, with index 3 as cursor, index 0..2 as
precursors and index 4..12 as postcursors. Time samples must agree between the
cases, but their calibration-source identities may differ. The same TX and RX
reference planes, normalization, calibration contract and equalizer identities
must match. Live freshness is the caller's responsibility; comparison against
old Results alone cannot prove that the UI source has not changed.

Outputs are ALWAYS `Status::Warning` with `NOT_IEEE_COM` and an explicit
`VICTIM_SELF_ISI` role. A caller-declared volts-per-unit-symbol number and
metadata text do not verify a physically calibrated receiver waveform; Sdd21 is
NOT assumed to be Vout/Vin.

## Mathematical contract

For signed receiver-equivalent taps `h[k]` in V, let the cursor be `h[0]`.
The indices `k=-3,-2,-1,+1,...,+9` are the 12 **self-ISI** coefficients.

Assuming *independent, equiprobable* ideal PAM4 symbol levels
`L in {-1,-1/3,+1/3,+1}`, `E[L] = 0` and `E[L^2] = 5/9`.
Under **that assumed model only**:

- `precursorRmsV = sqrt((5/9)*sum(k<0, h[k]^2))`
- `postcursorRmsV = sqrt((5/9)*sum(k>0, h[k]^2))`
- `totalIsiRmsV = sqrt((5/9)*sum(k!=0, h[k]^2))`
- `worstCaseIsiBoundV = sum(k!=0, abs(h[k]))`
- `nominalPam4HalfSpacingV = abs(h[0])/3`

RMS uses stable hypot accumulation to avoid intermediate square overflow.
The worst-case bound is a deterministic triangle-inequality upper bound for
symbols with magnitude at most 1, NOT a tail quantile. Half-spacing is merely
the nominal distance from a PAM4 ideal level to its halfway decision boundary,
NOT measured eye height, COM signal, BER/DER or qualified margin. Neither RMS
nor half-spacing accounts for pattern correlations, decision errors, DFE
feedback, receiver adaptation or physical source voltage calibration.

## Fail-closed

Reject stale/non-Warning result, missing NOT_IEEE_COM diagnostic, unknown
upstream algorithm/version/model, unmatched SHA/revision/mapping/settings/
workspace+mapping generations, invalid Test identity, reference plane,
normalization/calibration/EQ mismatch, invalid volts-per-unit-symbol, malformed
time axis, conversion inconsistent with signed source pulse, contradictory
reported cursor or maximum, nonfinite values, zero/subnormal cursor,
RMS/voltage underflow or overflow and cancellation. Reject single-case
settings masquerading as a pair.

No synthetic noise or independent aggressor is constructed. Victim ISI is not
fed automatically to 012_20's independent PAM4 **aggressor** distribution and
no 012_17/012_18 DFE result is silently used to replace these uncorrected
self-ISI taps. The production `calculateCom()` remains `NotEvaluated`.

## Independent numerical qualification

One targeted `channel_com_victim_isi` CTest integrates actual 012_21 scaling
and checks these separately derived numerical values:

| Case | cursor V | nonzero precursor V | nonzero postcursor V | total RMS V | worst-case bound V | nominal half-spacing V |
|---|---:|---:|---:|---:|---:|---:|
| Test1 | 0.16 | -0.02 | +0.04 | 0.03333333333333333 | 0.06 | 0.05333333333333334 |
| Test2 | 0.20 | +0.10 | -0.08 | 0.09545214042184236 | 0.18 | 0.06666666666666667 |

Test cases also cover polarity reversal, all-zero ISI, very small representable
volts, enormous overflow, tampered conversion, stale/mixed source, mismatch in
settings/calibration/reference plane, timing, cancellation and the unchanged
production COM gate. Values are **synthetic mathematical Goldens**, not final
IEEE 802.3ck channel measurements or normative compliance values.

## Scope limits, completion evidence and references

- Exactly one new source/header, one CTest, CMake registration and this
  single document + one living-plan entry.
- No GUI, main, v1.3.5 tag, release, global search, FFT, COM dB or PHY verdict.
- Do not claim completion without checks on the exact PR HEAD, actual review,
  protected squash merge to integration and post-merge verification.
- [012_21 scaling contract](COM_Voltage_Scaling_012_21.md)
- [012_20 non-normative joint statistical tail](COM_Joint_Tail_012_20.md)
- [IEEE Std 802.3ck-2022 official publication](https://standards.ieee.org/ieee/802.3ck/7322/)
