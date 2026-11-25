# 012_47 — User preferences and workspace usability

Baseline: `02fbbd784855a5e607f67f266e1649b1d2918a99` (PR #110, integration/v1.4.0).
Baseline post-merge workflows: 45563692440, 45563692442, 45563692453 all completed/success.
Candidate implementation only until required exact-head CI, review, protected squash merge and post-merge evidence are confirmed.

- 01: RL -10 dB, IL -3 dB, NEXT/FEXT -20 dB, disabled. TDR is an existing symmetric +/-10% tolerance; the lower bound is -10%, the upper bound +10%, and its enable remains false. Negative internal tolerance is invalid; no change to measurement, comparator or unit contract. Existing explicit project/preset values retain precedence; no new global electrical settings store.
- 03: one shared workspace/v1/lastDirectory; read fallback on missing/invalid/unusable path; suggested filename retained; update after successful project, Touchstone, mapping, preset and export operations, including ILD/ICN/Eye dialogs. Bundled sample and automated main validation do not update it. Existing explicit Save uses its project path. Network/filesystem latency and OS permission policies still apply.
- 06: new independent normal-size and maximized preferences; cursor screen, available geometry, first-launch maximize; never restore minimized or fullscreen startup mode. Existing project geometry and splitter/font keys retained. Native frame sizing/position, multi-monitor disconnect, DPI and physical OS interactions remain 012_50.
- Test: existing isolated workspace GUI target now checks literal expected default values, legacy explicit 0 and enable, preference roundtrips, empty/missing/corrupt path handling, centered/clamped negative-origin screen geometry, initial maximize and stored normal state. These checks are not claimed PASS until CI. Native dialogs and inaccessible-network path behavior are not simulated by these helper tests.
- 012_48: New/cache and sequential individual port setup. 012_49: integrated regression. 012_50: native and package dependency optimization. 012_51: final release candidate audit. 012_52: protected main and official release. Preserve COM DEFERRED, main and v1.3.5.
- Local Git DNS blocked (git ls-remote). Connector source review and exact-head CI are used; no local build or native proof claimed.

## First CI and review correction

Initial PR HEAD `5c8a16039d9dfdc47dfd0de99775009ea56fd1d1`: Windows x64 workspace_layout and native macOS workspace check failed at the negative-screen-center assertion; Windows ARM64 build and CodeQL passed. Fix uses nonnegative extent differences for deterministic centering, including odd dimensions, rather than QRect signed-center rounding. Codex findings: close mapping CSV stream before treating output as successful (Linux /dev/full regression), capture current normal geometry on every fullscreen entry, including maximized entry. New exact-head CI and review remain mandatory.
