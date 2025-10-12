# iOS Application Support Setup Guide

This guide explains how to set up iOS application support in Goliath using ipasim.

## Overview

Goliath supports running iOS applications through [ipasim](https://github.com/ipasimulator/ipasim), which is an iOS emulator rather than a compatibility layer. This approach provides better reliability and compatibility for complex iOS applications compared to translation-based solutions.

## Prerequisites

- Linux system with Goliath installed
- Git (for cloning ipasim repository)
- Build tools (cmake, make, gcc/clang)
- iOS application files (.ipa format)

## Installing ipasim

### Method 1: From Source (Recommended)

1. **Clone the ipasim repository:**
   ```bash
   git clone https://github.com/ipasimulator/ipasim.git
   cd ipasim
   ```

2. **Follow the build instructions from the ipasim project:**
   ```bash
   # Check the ipasim README for specific build instructions
   # as they may vary based on the current version
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Install ipasim to your system:**
   ```bash
   sudo make install
   ```

4. **Verify installation:**
   ```bash
   which ipasim
   ipasim --version
   ```

### Method 2: Package Manager (if available)

Check if your distribution provides ipasim packages:

```bash
# Ubuntu/Debian
apt search ipasim

# Fedora
dnf search ipasim

# Arch Linux
pacman -Ss ipasim
```

## Configuration

### 1. Verify Goliath Integration

Test that Goliath can detect iOS applications:

```bash
# Run the test script (if available)
./test-goliath.sh

# Or test manually with a dummy .ipa file
touch test.ipa
./goliath-launch.sh test.ipa
```

You should see output indicating that Goliath detected an iOS application and is attempting to launch it with ipasim.

### 2. Environment Setup

Ensure ipasim is in your PATH:

```bash
echo $PATH
which ipasim
```

If ipasim is not in your PATH, add it:

```bash
export PATH="/path/to/ipasim/bin:$PATH"
# Add this line to your ~/.bashrc or ~/.profile for persistence
```

## Usage

### Running iOS Applications

1. **Obtain iOS application files (.ipa):**
   - Extract from iTunes backups
   - Download from legitimate sources
   - Use your own developed applications

2. **Launch with Goliath:**
   ```bash
   # Basic usage
   ./goliath-launch.sh MyApp.ipa
   
   # With full path
   ./goliath-launch.sh /path/to/MyApp.ipa
   
   # With arguments (if supported by the app)
   ./goliath-launch.sh MyApp.ipa --some-argument
   ```

3. **Goliath will automatically:**
   - Detect the .ipa file format
   - Verify ipasim is available
   - Launch the application using ipasim

### Example Session

```bash
$ ./goliath-launch.sh Calculator.ipa
Detected iOS application: Calculator.ipa
Launching with ipasim...
[ipasim output follows...]
```

## Troubleshooting

### Common Issues

1. **"ipasim command not found"**
   - Ensure ipasim is installed and in your PATH
   - Try running `which ipasim` to verify installation

2. **"File does not exist"**
   - Check the path to your .ipa file
   - Ensure the file has the correct .ipa extension

3. **Application fails to launch**
   - Check ipasim logs for specific error messages
   - Verify the .ipa file is not corrupted
   - Some iOS applications may not be compatible with emulation

### Getting Help

1. **Goliath Issues:**
   - Check the main Goliath documentation
   - File issues in the Goliath repository

2. **ipasim Issues:**
   - Consult the ipasim documentation: https://github.com/ipasimulator/ipasim
   - File issues in the ipasim repository

3. **iOS Application Issues:**
   - Check application compatibility with ipasim
   - Some applications may require specific iOS versions or features

## Limitations

- Not all iOS applications may work perfectly in emulation
- Performance may be slower than native execution
- Some iOS-specific features may not be fully supported
- DRM-protected applications will not work

## Advanced Configuration

### Custom ipasim Settings

You can modify the Goliath launcher to pass custom arguments to ipasim:

1. Edit `goliath-launch.sh`
2. Locate the iOS detection section
3. Modify the `exec ipasim "$app" "$@"` line to include additional parameters

Example:
```bash
exec ipasim --verbose --debug "$app" "$@"
```

### Integration with Development Workflow

For iOS developers, you can integrate Goliath into your build process:

```bash
# Build your iOS app
xcodebuild -project MyApp.xcodeproj -scheme MyApp archive

# Export as .ipa
xcodebuild -exportArchive -archivePath MyApp.xcarchive -exportPath . -exportOptionsPlist export.plist

# Test with Goliath
./goliath-launch.sh MyApp.ipa
```

## Contributing

If you encounter issues or have improvements for iOS support in Goliath:

1. Test thoroughly with various iOS applications
2. Document any compatibility issues
3. Submit pull requests with improvements
4. Help improve this documentation

## References

- [ipasim Project](https://github.com/ipasimulator/ipasim)
- [Goliath Documentation](README-goliath.md)
- [iOS App Distribution](https://developer.apple.com/documentation/xcode/distributing-your-app-for-beta-testing-and-releases)