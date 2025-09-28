
# Goliath Unified Compatibility Layer Specification

## Overview
Goliath is a unified compatibility layer that enables running Windows, macOS, Android, Linux, and legacy hardware (e.g., NES) applications on each other. It is based on an updated fork of Valve/Proton's Wine, with deep integration of:
- Wine (latest upstream)
- Darling (macOS compatibility)
- WSL (Linux subsystem)
- ATL (Android translation layer)
- Libretro (legacy/console app support)

## Requirements
- Update all Wine components to match the latest official Wine release, resolving conflicts and preserving custom changes.
- Migrate and embed Darling, WSL, and ATL code into the unified Goliath codebase, ensuring seamless integration and cross-compatibility.
- Integrate Libretro to enable running legacy hardware/console apps (e.g., NES) via Libretro cores.
- Refactor, unify, and ensure the codebase builds and runs as a single, complex, production-quality system.
- No stubs or pseudo-code; all code must be real, buildable, and follow the codebase structure and user specifications with 100% accuracy.

## Acceptance Criteria
- Goliath can run Windows, macOS, Android, Linux, and legacy hardware apps from a single unified launcher.
- All subsystems (Wine, Darling, WSL, ATL, Libretro) are fully integrated and buildable as part of Goliath.
- The codebase passes all tests and builds on supported platforms.
- All integration points are covered by automated tests.
- Documentation is updated to reflect the new architecture and usage.

## References
- [Wine](https://www.winehq.org/)
- [Valve Proton](https://github.com/ValveSoftware/wine)
- [Darling](https://github.com/darlinghq/darling)
- [WSL](https://github.com/microsoft/WSL)
- [ATL](https://gitlab.com/android_translation_layer/android_translation_layer)
- [Libretro](https://github.com/libretro/libretro-common)
