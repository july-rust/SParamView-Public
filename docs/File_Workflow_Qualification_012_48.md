# 012_48 — File/workspace and port mapping qualification

Status: IMPLEMENTATION CANDIDATE; exact-head CI, CodeQL, Codex, integration merge and post-merge CI required before COMPLETE.

Baseline: 012_47 integration SHA `7d8ef06b2e9525669eeb577a2a78f1f606b2d874`.

## Requirement 02 — File > New
- Standard New shortcut; busy legacy tasks cannot start a competing transition.
- Existing save/discard/cancel gate, including save failure and short-write protection.
- Fresh owned workspace directory; invalidate analysis contexts and legacy viewport generation before releasing handles and retiring the previous cache.
- No deletion of original sources, stored projects, exports or a parallel instance's workspace; cleanup failures are logged and surfaced in the status bar.
- Clear project, graph, comparison, channel and exportable result state. Restore fresh project defaults; preserve persistent preferences and successful working-directory updates.

## Requirement 05 — Individual file mapping
- Sequential asynchronous file import followed by file-specific modal review where existing detection requires it. Automatically proven metadata retains its existing behavior.
- Stage mapping edits; commit only after confirmation, with existing single-file review-later behavior retained on cancellation.
- In a multi-file selection, skip an individually cancelled mapping and continue; report parse failures and continue. Explicit progress cancellation stops the remaining batch without discarding already accepted files.
- Unique revision names preserve the existing duplicate policy. No cross-file mapping sharing. Mapping changes invalidate affected results and the comparison table; analyzer context generations invalidate previous inputs.

## Automated evidence
Extend the existing workspace executable with File/New action, cancel/save failure/save success, source preservation, cache rotation/parallel workspace isolation, repeated New, restored defaults/directory, and synthetic four-port sequence with individually edited mappings, cancelled middle file and a malformed file. The `012_48_file_workflow_qualified` JSON flag is emitted only after assertions pass.
Actual run evidence: pending. Native interactive save-file dialogs and physical monitor qualification remain in 012_50. Integrated regression is 012_49. COM remains DEFERRED / NOT_IEEE_COM. Main and official releases remain untouched.
