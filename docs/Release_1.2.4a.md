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
