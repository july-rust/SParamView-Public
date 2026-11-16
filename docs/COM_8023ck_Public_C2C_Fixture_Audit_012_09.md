# 012_09 — IEEE 802.3ck public C2C source channel inventory (2019 contribution)

**Scope:** safely identify real public C2C Touchstone candidate channels for future same-PHY COM golden qualification. It does NOT run COM or qualify any Test 1/Test 2 margin.

The official IEEE 802.3ck task-force [public tools and channels directory](https://www.ieee802.org/3/ck/public/tools/) links to a 2019 Keysight contribution describing 10, 16, 18 and 20 dB chip-to-chip models with impairments. The exact official archive is:

- https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip

Script: `scripts/ieee_3ck_public_c2c_corpus_audit.py` performs a **read-only**, size-constrained download; checks ZIP entry counts, traversal/symlinks, duplicate names and expanded member size; lists physical S4P or other SNP file extensions, counts independent candidate files, and computes SHA256 of the archive and up to 12 individual SNP members. No third-party channel bytes are uploaded, copied into this repository, or placed in a release. GitHub artifact contains only SHA-256 and text metadata.

CI unit tests use invented small Touchstone-like files solely to check parser and provenance behavior. They **are not** channel performance goldens.

This 2019 source is a pre-standard contribution, not an approved-final 2022 COM workbook or a trusted COM numerical output. Even if there are four distinct channels in this archive, valid Golden coverage still requires verified matching source layout, correct THRU/NEXT/FEXT pairing, the **same approved 2022 C2C parameter profile**, independent official COM reference code output for BOTH package cases, a separate C++ implementation output, numerical tolerances and license rights.

The qualification manifest MUST stay `REFERENCE_READY` with `cases=[]`, `parameter_values_qualified=false`, and no `calculateCom()` COM dB until those actual reference results are obtained and compared.

To run `python -m unittest discover -s tests/channel_analysis -p test_ieee_3ck_c2c_corpus_audit.py -v` then `python scripts/ieee_3ck_public_c2c_corpus_audit.py > evidence.json` in an environment with official IEEE HTTPS access.
