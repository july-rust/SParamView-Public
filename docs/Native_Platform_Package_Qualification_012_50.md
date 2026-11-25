# 012_50 — Native platform and release package qualification

Status: CANDIDATE — no native or physical acceptance claimed until evidence exists.

Baseline: integration `9bd704d1bcd973bb07047e7859ebbe24a1399aa3`, PR #113; post-merge CI 45602164527 and CodeQL 45602164519. Latest official v1.3.5 and main remain unchanged. Existing CI artifact sizes are not release ZIP sizes.

Windows: preserve complete QA bundle and optional CLI; stage deterministic slim user ZIP and separate full QA ZIP with identical source provenance. Only tests/ and its dedicated launcher are separated; runtime DLLs, plugins (including OpenGL/fallback), documentation, licenses, examples and unknown files stay. Inventory measures compressed and uncompressed size, hashes each member and binds both ZIPs. Exact user ZIP runs with native Windows platform and SDK paths removed on x64 and ARM64; release publication consumes the same verified ZIPs and native receipts. ARM64 native list includes ILD/ICN/Eye/common GUI and COM fail-closed regression while existing workspace tests cover 012_47–49.

macOS: version metadata derived from release version; run packaged Cocoa selftest without SDK plugin/library paths and exercise actual ZIP extraction and mounted DMG with signature verification. Existing ad-hoc signing is not notarization. No changes to analysis algorithms or COM freeze.

Pending: exact PR-head CI/CodeQL/Codex, size report from real ZIPs, merge and post-merge CI. Physical multi-monitor/DPI, manual startup-dialog input, interactive Excel/Calc import/save and real GPU driver coverage are NOT_TESTED unless separately evidenced. Native automation does not establish physical acceptance. No official release or main change in 012_50.
