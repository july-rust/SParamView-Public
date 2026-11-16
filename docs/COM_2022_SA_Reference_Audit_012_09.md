# 012_09 — Read-only audit of official IEEE 2022 sponsor-ballot COM C2C settings

**State: Historical source evidence only — not an approved-final IEEE 802.3ck numerical qualification.**

## Source identity

The official IEEE 802.3df November 2022 COM 3.90 engineering presentation lists a
2022-08-17 3ck SA 120F C2C configuration workbook:
`config_com_ieee8023_93a=3ck_SA_120F_C2C_08_17_2022.xlsx`.
This is a historical sponsor-ballot/reference tool, not the IEEE final standard.

- [Official IEEE Nov 2022 COM 3.90 presentation](https://www.ieee802.org/3/df/public/22_11/mellitz_3df_01_2211.pdf)
- [Official IEEE 802.3df November 2022 minutes and tool references](https://grouper.ieee.org/groups/802/3/df/public/22_11/minutes_3df_2211_approved.pdf)
- [Official 2022 reference archive download](https://www.ieee802.org/3/df/public/tools/COM/mellitz_3df_02_2211.zip)
- [Approved IEEE Std 802.3ck-2022 catalog](https://standards.ieee.org/ieee/802.3ck/7322/)

## Real executed 2022 reference-source receipt (read-only)

- Real GitHub Actions execution: [2022 SA C2C config audit #43834905502](https://github.com/july-rust/SParamView-Public/actions/runs/43834905502) **SUCCESS**, with standalone offline synthetic unit tests.
- IEEE archive SHA-256: `895857fbfb602c8ec10b221ab40a23d4b39244b4b57101dd27abc4b2a1a2fddc`.
- Located 2022 C2C workbook: `config_sheets_100G/config_com_ieee8023_93a=3ck_SA_120F_C2C_08_17_2022.xlsx`.
- Workbook SHA-256: `e676b3fb3cb3048f80c98deaa8faca1d03c13daa216c6259de26885e715ca925`.
- The tightened parser found **27** distinct parameter names with finite **nearby adjacent** literal numeric cells. This is provenance evidence, **not** 27 individually verified IEEE normative parameters.
- Example: `f_b = 53.125` in two workbook sheets; one sheet has `c(0)=0.54`, a second `c(0)=0.60`. Without an approved table/worksheet contract, such values must not be silently conflated, including copied config comments or formula values.
- No source ZIP/XLSX bytes were stored in the repository or CI artifact. Test-only data is explicitly synthetic.

## Implemented workflow

`scripts/ieee_com_390_sa_probe.py` fetches the official ZIP read-only with
byte/file-count/path constraints, identifies the exact C2C XLSX, computes
SHA256 of the external ZIP and XLSX, and extracts available named numeric
settings through the XLSX OOXML XML representation. No XLSX or third-party
source code is committed or uploaded; GitHub CI publishes only textual
provenance/report metadata. The output is always explicitly:
`HISTORICAL_INDEPENDENT_SOURCE_NOT_FINAL_NORMATIVE_LOCK`,
`normative_table_120F8_qualified=false`,
`ieee_com_cpp_numerical_goldens=0`.

Offline synthetic parser tests prove shared strings/row decode, ZIP traversal
protection, missing worksheet failure and Excel-column address decoding.

To reproduce:

```sh
python -m unittest discover -s tests/channel_analysis -p test_ieee_com_390_sa_probe.py -v
python scripts/ieee_com_390_sa_probe.py > 2022-com-config-evidence.json
```

## What remains genuinely blocked

Source access and byte SHA do not establish the complete numeric values
of **final published** IEEE 802.3ck-2022 Annex 120F.4.1/Table 120F-8,
exact approved package Test1/Test2 reference program output, or 3 real
independent same-PHY channel goldens compared to C++ COM.
IEEE standard conformance and `012_09=100% COMPLETE` are never inferred
from a historical tool workbook or from successful workbook parsing.
Do not modify the production COM manifest to LOCKED on this evidence alone.
