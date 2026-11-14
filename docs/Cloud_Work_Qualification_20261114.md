# Independent cloud Work qualification, 2026-11-14 KST

## Baseline and scope

Official v1.3.4: `d0e9c1878ee9f90c30fc56c2e67723b73ad9b972`.
The official tag and all published assets are immutable and were not replaced.
The Linux executables used here were built from that exact source before any
edits. They are not the released Windows/macOS packages.

Environment: Debian 13.6 x86_64, Qt 6.8.2, X11 cloud desktop and LibreOffice
25.2.3.2 (Debian packages extracted into an isolated work directory).
The preinstalled LibreOfficeDev 26.8 headless runtime was not used for the GUI
qualification. Package relocation changes only local configuration paths.

## Confirmed baseline defect

A C++ probe linked to the unmodified production `si_core` exported 29 channels.
Each channel has four text fields and four port fields: 116 text and 116 port
values in total. The original CSV read back with zero mismatches.

The real Calc GUI opened the CSV with UTF-8, detected comma separator, standard
column types, quoted-fields-as-text OFF, special-number detection OFF,
scientific-notation detection ON and formula evaluation ON. Save As retained
Text CSV. After importing the saved CSV with production `readMappingCsv`:

| Original | After Calc save | Affected text fields |
| --- | --- | --- |
| CRLF followed by `=1+2` | LF followed by `=1+2` | 4 |
| `line` + CRLF + `break` | LF in place of CRLF | 4 |
| `00123` | `123` | 4 |
| `12345678901234567890` | `1.23456789012346E+019` | 4 |
| `1E10` | `10000000000` | 4 |
| `line` + CR + `break` | LF in place of CR | 4 |

There were 24 changed text values; port values did not change. The CR/CRLF/LF
cases collapsed to duplicate names. The unmodified SParamView GUI's Import
Mapping CSV action reproduced `Mapping needs unique non-empty names and IDs`.
This rejection is correct validation, but does not repair the lost data.
Without a duplicate, numeric label/ID changes can remain valid and undetected.

An ODS saved from the same live Calc document contained zero formula cells.
This establishes the tested formula-leading strings stayed text under these
settings, not universal safety for every spreadsheet version or option.

## Proposed correction (unreleased)

New exports use the explicit `v3-text-percent` marker. All four text columns
start with the literal `text:` prefix. ASCII bytes through space, DEL and `%`
are percent encoded; UTF-8 letters and ordinary punctuation remain readable.
Ports remain plain integers. Examples:

| Original SParamView value | CSV cell text |
| --- | --- |
| `00123` | `text:00123` |
| `=1+2` | `text:=1+2` |
| `line` + CRLF + `break` | `text:line%0D%0Abreak` |
| `100%` | `text:100%25` |
| empty metadata | `text:` |

When editing text cells in a spreadsheet, retain the `text:` prefix and use
`%25` for a literal percent sign. Prefer the Channel Mapping editor for normal
edits. The prefix/escapes are removed only by the v3 decoder. Malformed percent
escapes or missing prefixes are rejected. This is a transport encoding, not an
integrity signature or protection against deliberate edits.

Legacy unmarked and v2 CSV decoding remains unchanged. Official v1.3.4 rejects
v3 as an unknown format; it must not silently misread a newer file. Do not
distribute these new files to v1.3.4 users before a new release is approved.

The patched exporter underwent the same real Calc GUI load/Save As test. All
29 rows, 116 text values and 116 port values matched the original exactly on
production-core reimport. This evidence is specific to LibreOffice 25.2.3.2 on
Linux; Microsoft Excel and native Windows/macOS spreadsheet GUIs remain
unverified.

## Startup and cache qualification

- Actual unmodified SParamView executable: a regular file occupying the cache
  root produced a visible `SParamView - Startup failed` critical dialog.
- Text identified the failed directory and advised checking cache location,
  folder permissions and disk space. The OK button dismissed the dialog and
  the process exited with code 1. The sentinel file was preserved.
- Offscreen `--selftest` and minimal-platform ordinary launch failed with
  code 1 without waiting for a modal dialog.
- `session_cache_lifecycle` passed all four constructor fault locations,
  registry contention, ownership and cleanup checks.
- A real GUI load/close completed with code 0 and left no session directory,
  owner lock or registry lock in its dedicated cache root.
- Qt fatal aborts before QApplication, out-of-memory and arbitrary callback
  exceptions are not established by these tests. Linux X11 observations are
  not evidence of physical Windows/macOS dialog behavior.

## Test provenance

- Baseline local Release core: 31/31 CTests passed.
- Baseline local Qt build: 49/49 CTests passed.
- Patched local core: 31/31 CTests passed, including numeric strings, line
  endings, percent literals, legacy/v2 compatibility and damaged-v3 rejection.
- Official baseline workflow run `43413775544` was independently inspected:
  Windows x64, Windows ARM64 build, native ARM64 packaged-binary gate,
  macOS arm64, core and publication jobs were successful. These are existing
  release results, not a new interactive desktop run.
- PR CI results must be read from the current PR checks; no pending job is
  considered successful. Publishing/retagging is outside this qualification.

Source: [official release](https://github.com/july-rust/SParamView-Public/releases/tag/v1.3.4),
[baseline workflow](https://github.com/july-rust/SParamView-Public/actions/runs/43413775544).
The accompanying qualification evidence bundle contains CSV/ODS files,
comparison logs, CTest logs, GUI screenshots and SHA-256 hashes.
