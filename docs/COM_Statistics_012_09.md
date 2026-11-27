# 012_09 — Deterministic finite-state PAM4/Gaussian statistical noise core

**Status: mathematical component implemented; IEEE 802.3ck COM still NOT_EVALUATED.**

## Exactly what is calculated

Input parameters are an explicit positive Gaussian noise RMS voltage, a
specified **one-sided** target tail probability, and up to eight independent
equi-probable PAM4 tap voltage magnitudes. Every tap has four equally likely
normalized levels (-1,-1/3,+1/3,+1), independent between taps.

The resulting random noise is the sum of the independent discrete PAM4
contributions and a zero-mean Gaussian random variable. The solver enumerates
all 4^n states exactly, averages their Gaussian Q-function tails using
compensated summation, and bisects the positive one-sided voltage quantile
such that P(noise greater than quantile) equals the given target. It computes
the analytic total variance sigma^2 + (5/9)*sum(tap^2) independently.

This is a real deterministic noise/ISI probability kernel; it does NOT
substitute a Gaussian-RMS approximation for a non-Gaussian ISI mixture.

## Tests and evidence

- Pure Gaussian at one-sided p=1e-5, independent standard normal inverse
  survival z=4.264890793922825.
- Single PAM4 tap compared with independent 4-Gaussian closed-form tail
  expression; exact 16 states for 2 taps and 65536 for 8 taps.
- Independent tap permutation/sign symmetry, double-voltage scale
  invariance, stricter error target and higher quantile, analytic variance.
- Nonfinite/zero RMS, invalid probability, ninth tap, overflow, and
  cancellation all fail closed.

Reproduce with `ctest --test-dir build -R channel_com_statistical_noise`
after building the C++ core with `-DSI_BUILD_GUI=OFF`.

## Why this is not a complete IEEE 802.3ck COM solver

The actual IEEE 93A procedure involves a significantly richer receiver
noise PDF and ISI/crosstalk/jitter/equalizer optimization; it is not an
eight-tap equiprobable Gaussian-mixture model. This component provides a
fully specified numerical primitive, but is not qualified as the normative
IEEE noise histogram, FOM, channel margin dB, or PHY PASS/FAIL.

`calculateCom` continues NOT_EVALUATED/UNSUPPORTED with no numeric COM value.
The production IEEE 2022 final profile and at least 3 independent same-PHY
goldens for both Test1 and Test2 are still mandatory; no 1.4.0 release.

## Sources

- [Approved IEEE Std 802.3ck-2022](https://standards.ieee.org/ieee/802.3ck/7322/)
- [Official IEEE 802.3 COM reference](https://opensource.ieee.org/802-com/com_code)
- [IEEE public COM engineering explanation](https://www.ieee802.org/3/ck/public/18_05/ran_3ck_01_0518.pdf)
