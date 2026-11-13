# SParamView Stage 2 — Mapping CSV formula safety (unreleased candidate)

**NOT INCLUDED in the already-published v1.3.3 binaries.** This document records a Stage 2 candidate based on v1.3.3. The existing tag, release assets and checksums remain untouched. A subsequent versioned release is necessary before these modifications are described as shipping.

## Verified code paths

- src/mapping.cpp: arbitrary channel names, groups, aliases and IDs enter CSV. CSV quoting alone does not prevent spreadsheet formula interpretation.
- src/mapping.cpp: legacy importer trims data and did not accept quoted embedded CR/LF.
- src/export.cpp and src/cli.cpp: raw trace CSV contains numeric or controlled-vocabulary fields; left unchanged.
- src/report.cpp: XLSX text cells use inlineStr, not formula XML elements; left unchanged.

## Mapping CSV v2 wire format

- Header adds a final column named SParamViewCSV. Every row ends in v2-text-escaped.
- Text fields are RFC-style quoted. Interior quotes are doubled.
- A protective apostrophe is prepended to strings starting with a literal apostrophe, a leading ASCII control or BOM, or leading whitespace followed by =, +, -, @ or selected fullwidth operators.
- Only files explicitly identified with the v2-text-escaped marker are unescaped on import. Unmarked legacy CSV values are never stripped of apostrophes.
- V2 preserves leading/trailing spaces, empty strings, CR/LF, commas, quotes, and exact UTF-8 bytes across repeated export/import.
- Legacy empty ID and group defaulting is retained; v2 stores the actual empty text.
- Unknown versioned text encoding is rejected.

## Compatibility and limits

- Existing Mapping CSV continues to load using the historic trimming and default field semantics. Quoted multiline fields are newly supported.
- Older versions ignore the extra v2 column but may show the protective apostrophe in names imported from v2 CSV.
- CSV opening and re-saving in Microsoft Excel or LibreOffice may change cell types, apostrophes, and/or quoting. The protective prefix is a defense-in-depth measure and cannot be claimed to be universally secure across all spreadsheet round trips.
- Spreadsheet-specific load/save/reopen validation is NOT VERIFIED without separate tests performed in those applications.
- The v2 marker is a serialization version, NOT an integrity signature. Legacy or manually edited CSV is untrusted input.
- Keep original exported CSV as the canonical machine-readable artifact if exact roundtrip is necessary.

## Automated coverage required

- v133_csv_roundtrip_security CTest: formula starts, escaped apostrophes, Unicode, control characters, embedded newlines, quotes, BOM, numeric ports, legacy values, stable repeat exports, unclosed quotes and unknown version marker.
- Existing core, GUI, platform and sanitizers must continue to pass.
- Native Windows ARM64 packaging must execute this new test.

## Release policy

The published v1.3.3 commit and assets must remain untouched. The existing publication script rejects new code at the same tag. Leave Stage 2 as an unmerged development PR until a subsequent versioned release is authorized.
