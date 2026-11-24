"""Independent finite graph oracle; no C++ imports or repository dependency."""
from fractions import Fraction

scores = dict(enumerate(map(Fraction, [0, 2, 1, 4, 5, 3])))
assert max(scores, key=scores.get) == 4

def local(start, values):
    trace, seen, scans = [], {}, 0
    def value(i):
        if i not in seen:
            trace.append(i)
            seen[i] = values[i]
        return seen[i]
    current = start
    value(current)
    while True:
        scans += 1
        neighbors = sorted(i for i in values if abs(current-i) == 1)
        winner = max([current] + neighbors, key=value)
        if winner == current:
            return current, trace, scans
        current = winner

assert local(0, scores) == (1, [0, 1, 2], 2)
assert local(3, scores) == (4, [3, 2, 4, 5], 2)
assert local(0, {0: 1, 1: 1, 2: 2}) == (0, [0, 1], 1)
assert max([0, 1, 2], key={0: 3, 1: 3, 2: 1}.get) == 0
# Middle order: zero, then negative before positive at equal magnitude.
assert sorted(range(-2, 4), key=lambda x: (abs(x), x)) == [0,-1,1,-2,2,3]
print("012_41 independent synthetic graph oracle PASS; NOT_IEEE_COM")
