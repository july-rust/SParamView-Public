# 012_43 — COM development freeze and release scope stabilization

Date: 2026-11-25 KST. Baseline: `331c9e9ed7d3ee253aea0ac34cb991196e649849` (PR #106, integration/v1.4.0). Status: implementation proposed; qualification pending actual PR and post-merge CI. No claim of COMPLETE.

- Before: four-feature release gate; disabled COM menu and callable manager window entry.
- After: three-analyzer release scope (ILD, ICN, simulated Eye), legacy analyses and previously requested UX; COM deferred, NOT_IEEE_COM, 012_42 REFERENCE BLOCKED. Preserve COM source, numerical tests and history unchanged.
- Production isolation: skip COM menu registration, reject COM activation and direct manager open calls. Keep the existing internal enum and numerical build targets. No new COM computation.
- Existing GUI regression now checks exactly three menu entries, absent COM action/window, and rejects an attempted COM enable/open; retains window lifecycle, exception, cancellation, numeric and export tests on supported windows.
- Active plan and README updated; old numbered plans remain historical. Main and v1.3.5 are untouched.
- Local checkout unavailable (GitHub DNS failure). Actual CI, review, PR merge and post-merge CI evidence remain pending. See PR and Actions linked from the resulting commit; do not infer native execution or numerical PASS from source review.
- Follow-up: 012_44 ILD final qualification per active plan.
