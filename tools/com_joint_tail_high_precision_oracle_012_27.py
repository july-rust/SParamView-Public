#!/usr/bin/env python3
"""012_27 offline full-mixture quantile oracle; NOT IEEE COM.

Requires mpmath and scipy ONLY when regenerating fixed Golden values.
Never imported by the application or any CTest. Set 85 decimal places
(>=80) and reconstruct every C++ input literal as an exact binary64
rational. Independent of all SParamView production implementations.
"""
import math
import mpmath as mp
from scipy.optimize import brentq
from scipy.special import erfc

CASES = (
    ("A", .02, .01, .007, (.012, -.009, .0075, -.005, .003, .0015), 1e-12),
    ("B", .02, .01, .007, (.012, -.009, .0075, -.005, .003, .0015), 1e-5),
    ("C", .02, .008, .004, (.007, -.003), .1),
)


def binary64_exact(value):
    numerator, denominator = float(value).as_integer_ratio()
    return mp.mpf(numerator) / denominator


def solve(record):
    name, rx, rj, dirac, taps, target = record
    mp.mp.dps = 85
    r, j, d, p = map(binary64_exact, (rx, rj, dirac, target))
    sigma = mp.sqrt(r*r + j*j)
    # C++ levels use double literals 1./3.; preserve those binary inputs.
    levels = tuple(map(binary64_exact, (-1., -1./3., 1./3., 1.)))
    shifts = [mp.mpf(0)]
    for amplitude in taps:
        a = binary64_exact(amplitude)
        shifts = [v + a*level for v in shifts for level in levels]
    if d:
        shifts = [v + side for v in shifts for side in (-d, d)]
    assert len(shifts) == (4**len(taps)) * (2 if d else 1)

    def tail(q):
        divisor = mp.sqrt(2) * sigma
        return mp.fsum(mp.erfc((q-s)/divisor) for s in shifts) / (2*len(shifts))

    def density(q):
        return mp.fsum(
            mp.exp(-((q-s)/sigma)**2/2) for s in shifts
        ) / (len(shifts)*sigma*mp.sqrt(2*mp.pi))

    # SciPy double-precision seed is computed independently from the C++
    # implementation; mpmath then resolves the result at 85-digit precision.
    float_shifts = [0.]
    for amp in taps:
        float_shifts = [v+amp*z for v in float_shifts
                        for z in (-1., -1./3., 1./3., 1.)]
    if dirac:
        float_shifts = [v+side for v in float_shifts
                        for side in (-dirac, dirac)]
    f_sigma = math.hypot(rx, rj)

    def f_tail(q):
        return sum(.5*erfc((q-s)/(math.sqrt(2)*f_sigma))
                   for s in float_shifts)/len(float_shifts)

    seed = brentq(lambda q: f_tail(q)-target, 0.,
                  dirac+sum(map(abs, taps))+12*f_sigma, xtol=1e-15)
    q = binary64_exact(seed)
    for _ in range(16):
        residual = tail(q)-p
        if abs(residual) < mp.mpf("1e-78"):
            break
        q += residual/density(q)
    assert abs(tail(q)-p) < mp.mpf("1e-74")
    print(name, "states=", len(shifts),
          "quantileV=", mp.nstr(q, 82),
          "target_as_binary64=", mp.nstr(p, 28))


if __name__ == "__main__":
    for case in CASES:
        solve(case)
