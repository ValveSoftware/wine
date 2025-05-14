# Goliath Unified Compatibility Layer

Goliath is a meta-compatibility layer that unifies Wine (Windows), Darling (macOS), and ATL (Android) to run applications from all major operating systems on Linux.

## Usage

    ./goliath-launch.sh <application> [args...]

- The launcher will auto-detect the application type and dispatch to the correct subsystem.
- Ensure `wine`, `darling`, and `atl` are installed and available in your `PATH`.

## How it works

- **Windows apps**: Dispatched to Wine
- **macOS apps**: Dispatched to Darling
- **Android APKs**: Dispatched to ATL

## Integration Notes

- This is a pseudo-merge: each subsystem is kept separate, but the launcher provides a unified entry point.
- You can add more sophisticated detection or configuration as needed.

## Extending

- To add more OS support, extend the detection logic in `goliath-launch.sh`.
- For deeper integration, consider adding build system hooks or submodules for Darling and ATL.

## References
- [Wine](https://www.winehq.org/)
- [Darling](https://github.com/darlinghq/darling)
- [ATL](https://gitlab.com/android_translation_layer/android_translation_layer)
