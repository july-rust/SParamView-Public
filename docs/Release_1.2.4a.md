# SParamView v1.2.4a

This maintenance release is based on the official v1.2.4 source and preserves its Touchstone analysis calculations, GUI workflows and three-platform release targets.

## Changes
- Remove implicit floating-point narrowing warnings in Qt graph color rendering (not RL/IL/TDR math).
- Rename TDR quadrature and local export/mapping/project variables to eliminate C++ name-shadowing warnings.
- Use the nondeprecated Qt 6.8 `QMenu::addAction(text, shortcut, receiver, slot)` overload.
- Upgrade the GitHub Actions used for checkout, caching, Python setup and artifact transfers to Node.js 24-native supported major versions.
- Skip deployment of optional Windows DXC components for this Qt Widgets-only application; validate the deployed Windows dependencies as before.
- Preserve three-part numeric CMake version 1.2.4, Windows fixed resource version 1.2.4.1 and macOS bundle numeric versions, while setting the program, visible metadata, packages and Git tag to **1.2.4a**.
- Keep unchanged calculation/reference engine behavior. Platform-native and sanitizer regression suites remain required for publication.

## Post-release CI and test improvements (2026-10-13)

The public [v1.2.4a release](https://github.com/july-rust/SParamView-Public/releases/tag/v1.2.4a) remains the current official binary version. The published tag is pinned to `139b05fb112aec3aba4637a32eec48d1cc796672`; **existing ZIP, DMG, user-guide download attachments, SHA-256 checksums and release status were not overwritten**. The following validation-only changes were merged later into `main` in [PR #7](https://github.com/july-rust/SParamView-Public/pull/7):

- Retain the GCC Release core checks, but run ASan/UBSan with Clang to avoid two GCC 13 standard-library optimization diagnostics without suppressing warnings. Each 27-test suite passed.
- Run macOS GUI checks on the native Cocoa platform rather than the limited Qt minimal plugin. All 36 macOS tests passed; no unsupported application-font or propagate-size-hint messages remained.
- Use authenticated GitHub CLI to fetch ARM64 test and official publication artifacts instead of the Node action path that emitted `DEP0005` on the native ARM64 runner. Keep provenance, architecture, digest, and native-package checks.
- Fix a 20 ms GUI test event-order race that could report `Manual rows lost` before the expected polarity dialog was handled. Verify all eight manual mapping columns and execute the fallback scenario 10 times on native Windows ARM64; all ten passed.
- Keep the official release number **v1.2.4a** and treat these as CI/source-document maintenance only. If application runtime or packaging code changes later, publish a new tag rather than silently replacing the existing binaries.

The [official main CI run #38383938009](https://github.com/july-rust/SParamView-Public/actions/runs/38383938009) passed all seven jobs. Its checked compiler, Qt-test and artifact-download warning messages were **0** in this run; this is observed CI evidence, not a guarantee that future toolchains or runners cannot introduce warnings.

## Warning policy
Builds must be audited for all new compiler warnings and workflow/runtime environment warnings. A passing CI check alone does not prove a warning-free build; distinguish application-code warnings, optional platform-component notices, native Qt offscreen/minimal plugin messages, GCC standard-library warnings and external GitHub Actions warnings.

## Release targets
- Windows x64
- Windows ARM64, with packaged-native ARM64 execution
- macOS Apple Silicon arm64

## Limitations
- macOS is ad-hoc signed; not notarized.
- Quick TDR remains Preview.
- Automated tests are not a substitute for independent engineering or instrument validation.
