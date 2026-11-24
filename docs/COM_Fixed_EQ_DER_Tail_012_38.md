# 012_38 — fixed EQ DER/tail reference alignment (2026-11-24 KST)

**PARTIAL until exact-head CI, independent review, protected merge and push CI. NOT_IEEE_COM / FINAL PHY REFERENCE BLOCKED.** Baseline PR #101 merge ad95a2a8be6e40a9aae0730ee6aabbd1a796ced0; 012_37 completion receipt and all three post-merge workflows verified.

## Reference audit

[COM 4.17 pinned mirror](https://github.com/tbermen/SiCoPR/blob/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75/matlab/com_ieee8023_4p17p0.m), official commit 47459695a169fdf1fd10c541a5b19c69a5ae9285, byte SHA previously authenticated by 012_36. No new official MATLAB execution or Annex text qualification claimed. Lines 10822 (DER_0 config), 598–611 (Create_Noise_PDF and first strictly greater lower-tail CDF bin, magnitude of signed voltage; no interpolation or factor of two), 1595–1639 (Eq. 93A-30 TX RMS, RSS of RX/TX/RJ, Gaussian discretization and sampled A_DD*h_J), 1646–1704 (self/other interference convolution, Eq. 93A-44/45, optional quantization, cumulative masses), 9392–9397 (finite normalized Gaussian mass grid with delta_y), 9857–9868 (different two-eye helper, not main CDF). The reference variable name specBER does not make DER a physical BER/SER conversion. Full Annex mapping, final PHY target, calibration, sampled jitter span, aggressor topology, finite grid/truncation/quantization and reference equivalence are not qualified.

## Bounded implementation

Explicit synthetic PAM4 signed residual ISI/NEXT/FEXT/sampled DJ taps, Gaussian RX/TX/RJ; no single Dual-Dirac peak substituted for reference DJ. Recalculate 012_37 synthetic FOM contract and reconcile each declared distribution owner and RMS (PAM4 variance 5/9); zero RJ explicitly declared. Unique underlying owner per tap and same case/source/settings/decision plane; independent assumptions are not calibration proof. RX+TX Gaussian norm internal to legacy receiver slot; RJ separate. Existing 012_20 engine, maximum six taps (64/16 states in fixtures, up to 4096 with DJ represented by taps; original engine cap 8192 unchanged), p=1e-12..0.1, no omitted taps in this adapter (reject over budget; existing 012_25 conservative omitted-tap path unchanged). Preserve noncertified numerical bracket. No official A_ni output. Actual 012_36 replay rejected via source eligibility gate, including stale propagation. Separately exercise strict CDF bin selection on complete explicitly supplied synthetic probability masses, no interpolation, renormalization or physical PDF generation; ordinary binary64 cumulative sum (not certified probability arithmetic). Limit 8192 bins; mass sum tolerance 1e-12, no silently truncated input.

## Independent numerical qualification

80 decimal digit mpmath bisection (270 steps on [0,1] V), finite Cartesian product of binary64 PAM4 levels {-1,-1/3,1/3,1}, exact binary64 input floats converted to mpf. For each shift sum, survival is erfc((q-shift)/(sqrt(2)*sigma))/2; average equally, sigma=sqrt(rx²+tx²+rj²). Frozen before implementation:

| Case | RX/TX/RJ RMS V | Signed taps V (ISI, XT, sampled DJ) | p | q V |
|---|---|---|---|---|
| TEST1 | .02/.003/.01 | .012,-.009,.007 | 1e-5 | .1079887811626682084252091718066610688 |
| TEST2 | .01/.006/.005 | .004,-.006 | 1e-8 | .0761489552215853697698779568723278462 |

Relative point tolerance 2e-10 (legacy double erfc root and extra hypot/rounding; no absolute voltage floor); reference root must be inside distinct existing bracket. RMS consistency 2e-12 relative. Exact binary fraction grid fixture [.0625,.0625,.75,.125] with bins -2..1, bin .01 V and p=.0625: index 1, -.01 V, CDF before .0625, at .125. Zero-mass plateau, first-bin crossing, invalid mass/voltage, wrong source/EQ/plane/owner, missing declarations, cancellation and production gate. Existing 012_26/27 extremes/plateau/variance and 012_25 omitted bounds remain regression coverage. Synthetic math oracle independent from C++; not independent official PHY execution.

CTest: channel_com_fixed_eq_der_tail, existing channel_com_fixed_eq_noise_jitter source gate, existing joint/combined tail regression. No new runtime dependency, GUI, optimizer, COM dB, production activation, main or releases. Document once for all platforms. Exact HEAD review/CI and merge/push receipts required.
