"""Offline 012_39 ratio oracle using the frozen 012_38 fixture and 80-digit solver."""
import mpmath as mp
from com_fixed_tail_oracle_012_38 import CASES, solve

mp.mp.dps = 80
for case, cursor in zip(CASES, ("0.03", "0.06")):
    name, *args = case
    _, q = solve(*args)
    signal = mp.mpf(cursor) * mp.mpf(".95") / 3
    print(name, "A_s", mp.nstr(signal, 70), "ratio_dB", mp.nstr(20 * mp.log10(signal / q), 70))
