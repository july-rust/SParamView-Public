"""012_40 independent static oracle (no C++ dependency or PHY claim)."""
from fractions import Fraction
from itertools import product
from collections import Counter

ffe = list(product(range(-4, 1, 2), range(0, 11, 2),
                   range(-28, 1, 2), range(-10, 1, 2)))
counts = Counter(100 - sum(map(abs, taps)) >= 54 for taps in ffe)
assert len(ffe) == 1620 and counts[True] == 1605 and counts[False] == 15
assert next(i for i, taps in enumerate(ffe)
            if 100 - sum(map(abs, taps)) < 54) == 270
assert 21 * 5 * len(ffe) == 170100
assert 21 * 5 * counts[True] == 168525
assert 2 * counts[True] == 3210
assert Fraction(1, 53125000000 * 32) == Fraction(1, 1700000000000)
assert sorted(range(-2, 4), key=abs) == [0, -1, 1, -2, 2, 3]
print("012_40 independent static oracle PASS (NOT_IEEE_COM)")
