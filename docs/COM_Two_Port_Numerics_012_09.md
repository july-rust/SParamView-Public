# 012_09: 2-Port Network Physics — Analytically Checked, Not COM Qualified

**Status: non-normative package physics implementation; 012_09 overall remains BLOCKED.**

## Implementation

The new `ComTwoPort` library stores complex ABCD matrices and provides ordered
cascade, passive complex series impedance and shunt admittance, uniform lossy
transmission line, and bidirectional S11/S21/S12/S22 conversion for equal
real reference impedances. It is independent C++20 code and does not depend
on Python, MATLAB, Octave, or Qt.

The loss model accepts **explicit** attenuation (Np/mm) and group delay
(s/mm). It does not pretend that these values are the standard's complex
frequency-dependent attenuation model. Therefore it is not an IEEE device
package implementation by itself.

## Physically independent analytic acceptance

- Identity ABCD -> S11=0 and S21=1.
- A series resistor R at Zref has S21=2*Zref/(2*Zref+R),
  S11=R/(2*Zref+R).
- A shunt admittance Y has S21=2/(2+Y*Zref),
  S11=-Y*Zref/(2+Y*Zref).
- A matched line has S21=exp[-(alpha+j*2*pi*f*tau)*length]
  and S11=0.
- Two cascaded matched lines have the square of that transfer.
- A mismatched lossless line conserves |S11|^2+|S21|^2=1
  while retaining reflection.
- Cascade order changes directional reflections as expected.
- Nonfinite, negative passive coefficients, invalid Zref, and overflow
  fail closed.

Test 1/2 C2C nominal **TX/RX package line lengths** are represented
distinctly as 13/11 mm and 31/29 mm. These isolated values are not sufficient
to claim the entire final Table 120F-8 is qualified.

## Why full 012_09 is still NOT COMPLETE

This code has no 4-port-to-mixed-mode package/board cascading, full
IEEE 93A.1/120F pulse/ISI/noise/PAM4 and EQ optimization, C++ COM ratio,
or independent same-PHY reference COM values for three channels and both
package cases. The public COM 3.70 spreadsheet is a **pre-ratification
draft** and may differ from the final 802.3ck-2022 table.

`calculateCom` continues to return NOT_EVALUATED; no GUI or v1.4.0 release.

## Run

`cmake -S . -B build-core -DSI_BUILD_GUI=OFF`

`cmake --build build-core --target si_com_two_port_tests`

`ctest --test-dir build-core -R channel_com_two_port_physics --output-on-failure`

## Traceable sources

- [IEEE final 802.3ck-2022 catalog](https://standards.ieee.org/ieee/802.3ck/7322/)
- [Official IEEE 802.3ck public engineering presentations](https://www.ieee802.org/3/ck/public/)
- [2020 IEEE public 120F parameter comparison](https://www.ieee802.org/3/ck/public/adhoc/sept23_20/wu_3ck_adhoc_01a_092320.pdf)
- [Reference and license scope](COM_License_Review_012_02.md)
