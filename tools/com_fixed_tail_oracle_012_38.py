"""Offline synthetic 012_38 numerical oracle; never reads C++ output.
Requires mpmath, not a runtime or CTest dependency.
"""
import itertools
import mpmath as mp

mp.mp.dps = 80
LEVELS = tuple(map(mp.mpf, (-1.0, -1.0 / 3.0, 1.0 / 3.0, 1.0)))
CASES = (
    ("TEST1", .02, .003, .01, (.012, -.009, .007), 1e-5),
    ("TEST2", .01, .006, .005, (.004, -.006), 1e-8),
)

def solve(rx, tx, rj, taps, p):
    rx, tx, rj, p = map(mp.mpf, (rx, tx, rj, p))
    sigma = mp.sqrt(rx**2 + tx**2 + rj**2)
    amplitudes = tuple(map(mp.mpf, taps))
    shifts = [sum(a * level for a, level in zip(amplitudes, levels))
              for levels in itertools.product(LEVELS, repeat=len(amplitudes))]
    def tail(q):
        return sum(mp.erfc((q - s) / (mp.sqrt(2) * sigma)) / 2
                   for s in shifts) / len(shifts)
    lo, hi = mp.mpf(0), mp.mpf(1)
    for _ in range(270):
        mid = (lo + hi) / 2
        if tail(mid) > p:
            lo = mid
        else:
            hi = mid
    q = (lo + hi) / 2
    assert abs(tail(q) - p) < mp.mpf("1e-74")
    return sigma, q

if __name__ == "__main__":
    for name, *args in CASES:
        sigma, q = solve(*args)
        print(name, "sigma", mp.nstr(sigma, 70), "q", mp.nstr(q, 70))
