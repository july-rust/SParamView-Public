# 012_57 localization qualification — partial candidate

Baseline: integration/v1.4.0 at 952aa24f05ecb08e59f59306a02b52b449e20796 (PR #120); release gate #121 remains open.

## Implemented
- Catalog entries: 876 -> 886. Independent connector-side literal inventory: duplicate keys 0, blank strings 0, placeholder mismatches 0. This is a catalog check, not complete UI coverage.
- Translate explicit nested mapping/project diagnostic wrappers, signal type evidence, TDR quality notes, and analysis summary details; translate file errors before attaching filenames. Preserve numeric fields, opaque identifiers and existing serialization. No numerical engine changes.
- Localize ILD hover tooltip and deferred cache cleanup status.
- Workspace regression checks all catalog entries and selected composite diagnostics, including opaque filenames and TDR numbers.

## Review and limitations
- Audited selected GUI and core source literals, including mapping, Touchstone, cache, ILD/ICN/Eye and TDR. This is not an exhaustive AST or runtime inventory. Unknown diagnostic text is intentionally left unchanged; arbitrary composite coverage remains unverified.
- Reviewed PR #120 CodeQL thread topics (documentation, unused static variable and shadowing). Full finding-by-finding disposition and new CodeQL results remain outstanding.
- Local git clone failed due to DNS/network restrictions; source edits made using GitHub connector. No local compilation or GUI execution claimed.
- Native package, physical display/high-DPI, actual Touchstone workflows, full localization and new exact-head CI remain unqualified. Do not merge or close #121 until required evidence and coverage are complete. No main/tag/release changes.

## Follow-up: verified previous candidate and remaining result presentation
- Exact previous head 24b3b7121651d9e131742e8cffda1bfe3401e0cd: three-platform CI 45814388515 and CodeQL 45814388523 passed. Job details confirm Windows x64 packaged GUI, native Windows ARM64 packaged regression/GUI, macOS GUI and packaged self-test; release publication skipped. Native ARM64 validation artifact 15115513886; artifact content has not been independently inspected here.
- Catalog now 911 entries (35 added since 012_56); static literal scan: zero duplicate keys, blank strings, placeholder mismatches. Selected GUI source scan found no additional simple literal candidates except canonical metadata/product labels; this is not exhaustive runtime coverage.
- Main result rows now refresh metric/termination and tooltip presentation in place during language switching. Direction and TDR load prose translations added, plus selected verdict and marker notes. No changes to rank order, calculation, serialized project or CSV. Human-readable workbook evidence and TDR notes use the diagnostic display translation; workbook keys remain canonical.
- Added regression for row identity/order, selection, tooltip and metric text, and composite TDR load. New head requires its own CI/CodeQL.
- Physical desktop/high-DPI, complete runtime inventory and representative real Touchstone package workflow remain unverified. Requested-range composite messages and arbitrary external diagnostic tails still require qualification. Do not infer full translation from catalog count or previous CI.

## Follow-up: range warnings, exported Eye footer and source guard
- Exact head 70b2ee848764c67e3aa1880dc7a4264771cb8a2f passed three-platform CI 45898024472 (all platform jobs and native Windows ARM64 packaged-binary gate) and CodeQL 45898024503; publication skipped.
- Added translations for TDR validation failures, requested frequency/time coverage diagnostics (all four issue types), evaluated interval and NG/N/A verdict prose, and interpolated limit checks. Tests cover combined notes with unchanged numbers and unknown clauses.
- Eye PNG footer now translates its DC description and status; Korean simulated-only disclaimer explicitly states not measured. Canonical PNG metadata is unchanged.
- Catalog: 929 keys; duplicate 0, blank 0, placeholder mismatch 0. Independent connector-side scan of 20 known GUI files including the catalog found 190 explicit literal lookup occurrences, with zero missing keys after correcting the Eye footer. Added Python source guard to CTest to prevent missing literal keys. It is deliberately bounded, not proof of complete runtime coverage or all dynamic/implicit widget strings.
- PR #122 review reports the catalog function as poorly documented/large (CodeQL 770); no security failure, but maintainability disposition and review remain open. Earlier PR #120 review topics inspected; no claim of all findings resolved.
- Local clone retried and failed DNS resolution for github.com. No physical monitor, high-DPI, or representative real Touchstone package test performed here. Latest head CI and new source guard results must be verified separately. Keep #121 open and do not publish.
