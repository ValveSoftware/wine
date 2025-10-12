# Goliath Unified Compatibility Layer

Goliath is a meta-compatibility layer that unifies Wine (Windows), Darling (macOS), ATL (Android), and ipasim (iOS) to run applications from all major operating systems on Linux.

## Usage

    ./goliath-launch.sh <application> [args...]

- The launcher will auto-detect the application type and dispatch to the correct subsystem.
- Ensure `wine`, `darling`, `atl`, and `ipasim` are installed and available in your `PATH`.

## How it works

- **Windows apps**: Dispatched to Wine
- **macOS apps**: Dispatched to Darling
- **Android APKs**: Dispatched to ATL
- **iOS apps**: Dispatched to ipasim

## Running iOS Applications

The best way to run iOS applications is with [ipasim](https://github.com/ipasimulator/ipasim), which is an emulator rather than a compatibility tool.

### Prerequisites for iOS Support

1. Install ipasim from https://github.com/ipasimulator/ipasim
2. Ensure `ipasim` is available in your `PATH`
3. Have iOS application files (.ipa) ready to run

### Example Usage

```bash
# Run an iOS application
./goliath-launch.sh MyApp.ipa

# Run with additional arguments
./goliath-launch.sh MyApp.ipa --some-argument
```

The launcher will automatically detect `.ipa` files and dispatch them to ipasim for execution.

## Integration Notes

- This is a pseudo-merge: each subsystem is kept separate, but the launcher provides a unified entry point.
- You can add more sophisticated detection or configuration as needed.
- ipasim provides iOS application emulation rather than translation, making it more reliable for complex iOS applications.

## Extending

- To add more OS support, extend the detection logic in `goliath-launch.sh`.
- For deeper integration, consider adding build system hooks or submodules for Darling, ATL, and ipasim.

## References
- [Wine](https://www.winehq.org/)
- [Darling](https://github.com/darlinghq/darling)
- [ATL](https://gitlab.com/android_translation_layer/android_translation_layer)
- [ipasim](https://github.com/ipasimulator/ipasim)
