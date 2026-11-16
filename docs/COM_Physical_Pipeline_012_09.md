# 012_09 — End-to-end physical S4P channel through COM package cases

**Status:** The THRU package-sweep engine is physically computed. **Normative IEEE COM dB remains NOT_EVALUATED.**

## Numerical flow

`ComPhysicalThruRequest` carries physical S4P matrices in the fixed
`nearP,nearN,farP,farN` port order, actual source frequency samples,
explicit TX/RX balanced package electrical coefficients, direction,
bit rate, and a strict maximum mode-conversion tolerance.

The function `calculateComPhysicalThru` validates the measured DC, frequency
ordering, bandwidth, per-port physical reference, passivity and signal mode.
Each S4P sample is converted with the unitary 4x4 mixed-mode transform to
its full Sdd 2-port. For **both** 802.3ck C2C 120F Test1 and Test2
length selectors it builds the TX die-to-board and RX board-to-die
package circuits, then computes the wave-network Redheffer cascade
`TX-package -> physical differential THRU -> RX-package`, preserving
all internal reflections and complex phase.

The result contains two separate full-band complex differential transfer
traces and the actual Sdd11/Sdd22 reflections. A reversed direction
uses Sdd12 and switches source/destination reflection roles.

## Independent analytic tests

- Identity packages preserve measured Sdd21 including its phase.
- User-specified matched line attenuation and delay yield independently
  predictable `H_measured * exp[-(alpha+j*beta)*(Ltx+Lrx)]`,
  verified for both package cases with separate physical lengths.
- Package Test2 longer physical paths result in higher attenuation
  than Test1 under the explicitly supplied loss model.
- Reverse ideal reciprocal channel preserves Sdd12 phase and magnitude.
- Invalid active 4-port, inconsistent Zref, missing DC, insufficient
  measured band, missing data, illegal explicit length override,
  and worker cancellation fail closed.

## Essential limits

The method is a **real physical S-parameter computation**, but not an
approved-final 802.3ck package COM calculation. It does not implement
the full Annex 93A.1 PAM4 pulse/ISI/XTALK/noise/CTLE/DFE/TX FFE
optimization and COM dB ratio. It cannot claim Test1/Test2 PHY PASS,
independent Golden comparison, or `012_09=100% COMPLETE`.
Test1/Test2 lengths are only the available historical source-backed
parameters, not verified completeness of the published Table 120F-8.

Source and full engineering limitations:
- [IEEE Std 802.3ck-2022](https://standards.ieee.org/ieee/802.3ck/7322/)
- [Historical IEEE 3ck SA 120F C2C workbook SHA audit](COM_2022_SA_Reference_Audit_012_09.md)
- [COM numerical stage contract](Development_Plan_012_v1.4.0.md)

Reproduce Core CTest: `ctest --test-dir build -R channel_com_physical_pipeline --output-on-failure`.
