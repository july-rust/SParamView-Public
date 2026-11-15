# 012_02 — COM independent reference / Golden qualification

**Status: BLOCKED** (IEEE source tag and root license verified through CI; no official COM numerical oracle executed).

## Source audit

1. Official project: https://www.ieee802.org/3/ad_hoc/COM/
2. Official GitLab: https://opensource.ieee.org/802-com/com_code
3. Published COM 4.17.0 release announcement (2026-09-25): https://ieee802.org/3/ad_hoc/COM/email/msg00126.html
4. Public meeting list (2026-09-29): https://www.ieee802.org/3/ad_hoc/COM/public/telecon/index.html
5. Published 208-case MATLAB reference / Python / Octave comparison: **third-party presentation of comparisons**. Not a substitute for a locally reproduced independent oracle, and not an IEEE conformance certificate.

## Environment and negative evidence

| Attempt | Observation | Decision |
|---|---|---|
| Fetch IEEE GitLab through web | HTTP 418 / inaccessible | Local environment access blocked; GitHub Actions API fetch succeeded |
| Fetch GitLab raw LICENSE and API from execution container | DNS NameResolutionError | GitHub Actions independently fetched BSD-3-Clause license at tag `4.17.0`, sha256 `f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631` |
| Fetch project GitHub via execution container | DNS NameResolutionError | GitHub connector used for actual repository mutations |
| MATLAB/Octave/Python COM reference execution | No verified source/fixtures available | NOT RUN; do not claim COM numeric PASS |
| SHA-256 of official fixture(s) | Official fixture bytes unavailable | NOT GENERATED; no fake hashes |

## Pinned source evidence (actual GitHub Actions, not numerical verification)

- [Read-only upstream probe CI 43691394632](https://github.com/july-rust/SParamView-Public/actions/runs/43691394632): official tag `4.17.0` resolves to Git commit **`47459695a169fdf1fd10c541a5b19c69a5ae9285`**; GitLab API HTTP 200.
- Release file `release/com_ieee8023_4p17p0.m`, Git blob ID **`bdfc766226e4986f3428460b5fdb6818f3a3fcd5`**. Git blob ID is not SHA-256; API JSON response hash is not code-content hash.
- Repository root README and LICENSE both identify BSD 3-Clause; root LICENSE SHA-256 **`f5532297b5eeef9ef3236a38ecb5973d6521b542f567db15c4025e1f71d91631`**.
- Programmatic stage tests: **7 synthetic/provenance tests passed**, *not* actual IEEE COM numeric golden verification.

## Added reproducible evidence gate

- `scripts/com_reference_preflight.py`: stdlib-only **fail-closed** manifest/evidence verifier.
- `tests/fixtures/channel/com_reference/manifest.json`: truthful BLOCKED scaffold (no fake cases, commit, thresholds).
- `tests/channel_analysis/test_com_reference_preflight.py`: **synthetic-only** tests for missing source, hash mismatch, path escape, result disagreement and schema behavior.
- `.github/workflows/com-reference-preflight.yml`: dedicated Python preflight on PR and integration push.

Run from repository root:

```sh
python scripts/com_reference_preflight.py --mode schema --manifest tests/fixtures/channel/com_reference/manifest.json --root tests/fixtures/channel/com_reference
python -m unittest discover -s tests/channel_analysis -p test_com_reference_preflight.py -v
python scripts/com_reference_preflight.py --mode qualify --manifest tests/fixtures/channel/com_reference/manifest.json --root tests/fixtures/channel/com_reference
```

**Expected for current scaffold:** schema and synthetic tests can pass; `--mode qualify` must fail with code **2** because the official reference remains BLOCKED. **Do not run this strict expected-failure command as a success-only CI step.**

## Case acceptance before 012_02 completion

For at least three **real**, independently traceable IEEE reference cases:

- Each file physically present (or downloaded by a reproducible license-compliant test step), with checked SHA-256, file role (`thru`, `config`, etc.) and safe path.
- Each published expected COM value and permissible dB error drawn from **independent** source plus recorded citation, never invented from production output.
- Each reproduced `com_db` JSON generated from actual pinned reference execution, with `profile_id`, `reference_commit`, date, command, runtime/tool version and source logs.
- At least one NEXT/FEXT-containing fixture; full S-matrix physical port/direction validation and package/EQ/jitter/noise coverage required.
- Record numeric error per case and provide convergence evidence for tolerance. Current checker enforces integrity/COM scalar comparison, **not** mathematical correctness of COM algorithm, fixture semantic fitness, or provenance authenticity. Manual/independent audit remains mandatory.
- Any missing reference/privilege/standard or licensing evidence => `BLOCKED`, not PASS.

**Acceptance status now:** IEEE official source TAG/commit and BSD-3-Clause root LICENSE verified, but profile UNLOCKED, official fixture SHA UNAVAILABLE, reference run NOT EXECUTED, numeric oracle NOT VERIFIED, external fixture/dependency redistribution UNCLEARED. Consequently 012_02 remains **BLOCKED**, even if CI infrastructure tests succeed.
