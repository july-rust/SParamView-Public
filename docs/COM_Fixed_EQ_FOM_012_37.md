# 012_37 — bounded fixed EQ FOM, 2026-11-24 KST

**PARTIAL until final-head CI, independent review, protected merge and push CI. NOT_IEEE_COM / FINAL PHY REFERENCE BLOCKED.** Baseline aae222365ce300ddb4df8cc49ce873ae53c82cd0 (PR #100).

Reference: [COM 4.17 source](https://github.com/tbermen/SiCoPR/blob/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75/matlab/com_ieee8023_4p17p0.m#L3157-L3347), official commit 47459695a169fdf1fd10c541a5b19c69a5ae9285; 012_36 authenticated byte SHA-256 cdcd739ac13c9cc3ca59733fd938a015f040429655b5dbf5543c897789cfed9b. No new original Annex PDF or MATLAB execution claimed. OptFom_Calc_FOM non-C2M branch: Eq. 93A-36 voltage ratio; OptFom_Calc_Noise non-MMSE norm of ISI, jitter, XT, N, TX, ne. Lines 9655 (A_s=R_LM*cursor/(L-1)), 3269-3273 (TX SNR without C0 correction), 9538-9545 (ne=0 outside RX calibration). 287 symbol variance is 5/9 for four levels. C2C non-MMSE, no RxFFE, no calibration, no C0 correction, independently declared PAM4 only. No normative eligibility from declarations.

Input: per-case source/EQ/plane and complete declared synthetic RMS for four non-TX components; explicit inactive evidence, bounded unique owner keys across aggregate members and TX. Cursor and SNR derive TX RMS, no duplicate RLM. Reference ne is explicitly inactive in this branch. No arbitrary input replacement, no hidden default. All volts normal or explicit zero; nonrepresentable/subnormal results fail closed; stable hypot and exponent-aware log. FOM may be negative or zero. Separate source eligibility function rejects actual 012_36 replay even if usable, since finite observed ISI/h_J/crosstalk and RX span are not final qualified. Actual replay regression is in existing 012_36 CTest. No promotion to calculateCom, DER, optimizer, COM dB or GUI.

## Oracle frozen before implementation

Python standard-library Decimal, precision 70: for cursor c, RLM r, SNR s and four independent RMS n, a=Decimal(c)*Decimal(r)/3; t=Decimal(c)*10**(-Decimal(s)/20); v=(sum(Decimal(x)**2 for x in n)+t*t).sqrt(); f=20*(a/v).log10(). Inputs in V: Test1 c=.12,r=.95,s=30,n=[.004,.002,.003,.002]; Test2 c=.06,r=.9,s=20,n=[.010,.008,.007,.005]. No implementation results used. Normal-scale relative tolerance 2e-12 and absolute 2e-11 dB allow double rounding and platform libm variation, with no positive absolute voltage floor.

| Case | A_s V | TX RMS V | Total RMS V | FOM dB |
|---|---:|---:|---:|---:|
| TEST1 | .038 | .0037947331922020551984 | .0068847657912234022487 | 14.83788851559535252997 |
| TEST2 | .018 | .006 | .0165529453572468485932 | .7279444738622417583092 |

Analytic TX-only case: cursor 3 V, RLM 1, SNR 20 gives 20log10(10/3)=10.45757490560675 dB; SNR 0 gives 20log10(1/3)=-9.54242509439325 dB; SNR 20log10(3) gives 0 dB (rounding tolerance). Uniform 1e±280 scaling must preserve the score. Missing, duplicate, wrong case/plane, incomplete representations, malformed values, cancellation and production gate checked. Existing historical optimized 012_13 FOMs are not fixed-candidate Goldens.

Actual final PHY source eligibility remains blocked (012_32). No full jitter span, TX calibration, or normative waveform qualification is added. CI/review evidence must be attached to immutable PR/head/merge SHAs; document is authored once for all platforms.
