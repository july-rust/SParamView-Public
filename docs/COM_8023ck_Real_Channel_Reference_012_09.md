# 012_09 — Real IEEE public C2C channel reference COM prequalification

**Status:** pending actual numerical execution. No normative final 802.3ck COM qualification or SParamView native C++ COM dB.

## Reason

The IEEE 802.3ck task force supplied four distinct real-world C2C impairment groups with matched THRU, NEXT and FEXT S4P files. GitHub [read-only corpus audit #43866966952](https://github.com/july-rust/SParamView-Public/actions/runs/43866966952) downloaded the public archive and verified **12 S4P files** with unique SHA-256 hashes, in 10/16/18/20 dB source-labelled groups.

Official public 2022 COM 3.90 tool archive includes a historical 802.3ck SA 120F C2C workbook dated 2022-08-17. We can feed the **same pinned profile and real inputs** into an independent BSD-3-Clause COM implementation for actual two-package numerical references, instead of inventing COM margins.

## Executable reproducibility

Source and digest pins are in `scripts/com_3ck_c2c_real_reference.py`:

- Official C2C ZIP: `https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip`, SHA256 `670fe10711b5c530320f770d22a717387b161f2721a2203b27e84ebf7ac597e2`
- Official COM 3.90 ZIP: `https://www.ieee802.org/3/df/public/tools/COM/mellitz_3df_02_2211.zip`, SHA256 `895857fbfb602c8ec10b221ab40a23d4b39244b4b57101dd27abc4b2a1a2fddc`
- Exact archived C2C workbook SHA256 `e676b3fb3cb3048f80c98deaa8faca1d03c13daa216c6259de26885e715ca925`
- External SiCoPR 1.2.1 source pin `https://github.com/tbermen/SiCoPR/tree/cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75`, selected IEEE COM `4p17p0`
- Three scheduled independent *source-labelled* physical channel groups 10/16/18 dB, each with verified THRU/FEXT/NEXT unique SHA, and expected **two package-case outputs**. The fourth 20 dB input is SHA-pinned, reserved for additional diversity.

`python -m unittest discover -s tests/channel_analysis -p test_com_3ck_c2c_real_reference.py -v` verifies input identity and safe source handling before live calculation. The reference runner uses ephemeral temp directories; only SHA/provenance and numerical margin/FOM/timing outputs are uploaded as GitHub JSON artifacts. No IEEE/SiCoPR source code, workbook, or channel bytes are included in the SParamView source or release.

## Hard stops

1. Every input file SHA, archive SHA and source-code commit must match or the run fails.
2. The independently maintained Python port must actually produce **two finite COM and FOM values** from each channel; no estimates, placeholder numbers or 'PASS by no exception' reports.
3. The 2022-08-17 configuration is a historical IEEE-hosted engineering source, not independently locked against every published IEEE Std 802.3ck-2022 120F-8 parameter.
4. SiCoPR is a validated independent Python port of COM 4p17p0, **not an execution of the official Octave/MATLAB reference for these exact C2C cases**. Until official reference and C++ results agree numerically, this is only a candidate numerical oracle.
5. The C++ `calculateCom()` must remain fail-closed: NOT_EVALUATED, with no finite COM dB, no PHY PASS/FAIL, and no v1.4.0 release.

## Reference links

- [IEEE official 802.3ck public channels](https://www.ieee802.org/3/ck/public/tools/)
- [IEEE 2022 reference COM 3.90 archive](https://www.ieee802.org/3/df/public/tools/index.html)
- [SiCoPR implementation and published cross-version correlation report](https://pypi.org/project/sicopr/)
- [802.3ck COM golden preflight requirements](Channel_Analysis_COM_012_09.md)
