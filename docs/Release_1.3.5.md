# SParamView v1.3.5

## Corrections

- Mapping CSV no longer loses leading zeroes, long numeric text, scientific-notation labels, or CR/CRLF whitespace during the tested LibreOffice save/reopen path. v3 uses an explicit `text:` prefix and percent-encoded whitespace/control bytes. Port numbers stay numeric.
- A successful CSV import updates the stale mapping guidance.
- Legacy v1/v2 imports remain supported. **Official v1.3.4 cannot read v3 exports.** Prefer the application mapping editor; manual CSV editors must retain the prefix/escape rules.

## Qualification and limits

See [detailed qualification scope](Final_Qualification_1.3.5.md) and the attached
validation evidence for exact operating systems, commits, executable hashes,
logs, screenshots, exit codes and PASS/FAIL results.

The test-only executable `tests/si_failure_probe` compiles the same startup path
with injected failures. The shipping SParamView target has no enabled fault
hooks. Native Qt dialog automation dispatches mouse events to the visible OK
button; it is not a physical Windows/macOS desktop interaction claim.
Injected `std::bad_alloc` is not real OOM. Unguarded callback/fatal termination
is investigated through logging and next-launch recovery, not promised dialogs.

Excel desktop: **UNVERIFIED — Excel execution environment unavailable**.
Native Windows/macOS LibreOffice import/save dialogs remain unverified unless
an accompanying platform evidence record explicitly establishes them. Linux
LibreOffice results cannot stand in for those platforms or for Excel.
No claim is made about arbitrary event callbacks or all possible failures.

Existing v1.3.4 tag, release and assets remain unchanged. Numerical SI algorithms
are unchanged. The published release is permitted only after all platform gates
succeed for the exact release commit; a PR build alone is not a release.
