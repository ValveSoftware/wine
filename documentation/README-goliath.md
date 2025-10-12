# Goliath Unified Compatibility Layer

Goliath is a comprehensive meta-compatibility layer that unifies Wine (Windows), Darling (macOS), WSL (Linux), and ATL (Android) to run applications from all major operating systems on Linux and other Unix-like systems.

## Overview

Goliath provides a single entry point for running applications from different operating systems:

- **Windows applications**: Dispatched to Wine compatibility layer
- **macOS applications**: Dispatched to Darling compatibility layer  
- **Linux applications**: Dispatched to WSL (Windows Subsystem for Linux) compatibility layer
- **Android APKs**: Dispatched to ATL (Android Translation Layer)

## Usage

### Basic Usage

```bash
./goliath-launch.sh <application> [args...]
```

The launcher will automatically detect the application type and dispatch to the correct subsystem.

### Examples

```bash
# Run a Windows executable
./goliath-launch.sh notepad.exe

# Run a Linux binary
./goliath-launch.sh /usr/bin/ls -la

# Run a macOS application
./goliath-launch.sh /Applications/Calculator.app

# Run an Android APK
./goliath-launch.sh myapp.apk
```

## WSL (Linux) Support

Goliath includes a comprehensive WSL compatibility layer that provides Linux environment emulation similar to Microsoft's WSL.

### Features

- **Automatic path conversion**: Windows paths (C:\path) are converted to Linux paths (/mnt/c/path)
- **Environment setup**: Proper Linux environment variables (PATH, HOME, USER, etc.)
- **Signal handling**: Proper signal forwarding and process management
- **File system structure**: Creates basic Linux directory structure (/tmp, /var, /proc, etc.)
- **User management**: Integrates with system user accounts

### WSL Configuration

The WSL subsystem can be configured through environment variables:

```bash
export WSL_DISTRO_NAME="MyDistro"        # Set distribution name
export WSL_ENABLE_INTEROP=1              # Enable Windows interoperability
export WSL_ENABLE_DRIVE_MOUNTING=1       # Enable automatic drive mounting
```

### WSL API

The WSL subsystem provides a C API for integration:

```c
#include "wsl.h"

// Initialize WSL environment
wsl_init_config();
wsl_setup_environment();

// Convert paths
char *linux_path = wsl_convert_path("C:\\Windows\\System32");
// Result: "/mnt/c/Windows/System32"

// Launch Linux process
pid_t pid = wsl_launch_process("/bin/bash", argv, envp);
int status = wsl_wait_for_process(pid);
```

## Installation and Setup

### Prerequisites

Ensure the following are installed and available in your `PATH`:

- **wine**: For Windows application support
- **darling**: For macOS application support (optional)
- **atl**: For Android application support (optional)

### Building Goliath

```bash
./configure
make
make install
```

### Configuration

1. **Wine Configuration**: Run `winecfg` to configure Wine settings
2. **WSL Configuration**: Set WSL environment variables as needed
3. **Darling Setup**: Follow Darling installation instructions
4. **ATL Setup**: Follow ATL installation instructions

## How It Works

### Application Detection

Goliath uses multiple methods to detect application types:

1. **File magic numbers**: Reads binary headers to identify format
2. **File extensions**: Uses common extensions as fallback
3. **File command**: Leverages system `file` command for detection

### Dispatch Logic

```
Input Application
       ↓
   File Detection
       ↓
┌─────────────────┐
│  Windows PE?    │ → Wine
├─────────────────┤
│  macOS Mach-O?  │ → Darling  
├─────────────────┤
│  Linux ELF?     │ → WSL
├─────────────────┤
│  Android APK?   │ → ATL
└─────────────────┘
```

## Advanced Features

### Path Translation

Goliath automatically handles path translation between different operating systems:

- Windows → Linux: `C:\path\file` → `/mnt/c/path/file`
- Relative paths are preserved
- Network paths are handled appropriately

### Environment Integration

Each subsystem provides proper environment setup:

- **Wine**: Windows-like environment with registry, DLLs, etc.
- **WSL**: Linux environment with proper PATH, shell, locale
- **Darling**: macOS environment with frameworks and libraries
- **ATL**: Android runtime environment

### Process Management

Goliath provides unified process management:

- Signal forwarding between host and guest processes
- Proper exit code handling
- Resource cleanup on termination

## Configuration Files

### WSL Configuration

Create `~/.config/goliath/wsl.conf`:

```ini
[wsl]
distro_name = GoliathWSL
default_user = myuser
enable_interop = true
enable_drive_mounting = true
default_shell = /bin/bash

[environment]
LANG = en_US.UTF-8
TERM = xterm-256color
```

### Global Configuration

Create `~/.config/goliath/goliath.conf`:

```ini
[general]
auto_detect = true
verbose = false
log_file = ~/.local/share/goliath/goliath.log

[wine]
prefix = ~/.wine
debug = warn+all

[darling]
prefix = ~/.darling

[wsl]
config_file = ~/.config/goliath/wsl.conf

[atl]
data_dir = ~/.local/share/atl
```

## Troubleshooting

### Common Issues

1. **Application not detected**: Check file permissions and format
2. **WSL path issues**: Verify path conversion with `wsl_convert_path()`
3. **Environment problems**: Check environment variable setup
4. **Permission errors**: Ensure proper file system permissions

### Debug Mode

Enable verbose logging:

```bash
export GOLIATH_DEBUG=1
./goliath-launch.sh myapp
```

### Log Files

Check log files for detailed information:

- `~/.local/share/goliath/goliath.log`: General Goliath logs
- `~/.local/share/goliath/wsl.log`: WSL-specific logs
- `~/.wine/system.reg`: Wine registry and logs

## Integration Notes

### Pseudo-merge Architecture

Goliath uses a pseudo-merge architecture where each subsystem is kept separate but unified through:

- Common launcher interface
- Shared configuration system
- Unified logging and error handling
- Cross-platform path translation

### Extending Goliath

To add support for additional operating systems:

1. **Create subsystem directory**: `dlls/newsystem/`
2. **Implement loader**: Following the WSL example
3. **Update detection logic**: In `goliath-launch.sh`
4. **Add configuration**: In configuration files
5. **Update documentation**: Add usage examples

### Build System Integration

Goliath integrates with the existing Wine build system:

- Uses autotools for configuration
- Follows Wine DLL structure
- Maintains compatibility with Wine APIs

## Performance Considerations

### Overhead

Each compatibility layer adds some overhead:

- **Wine**: Moderate overhead for API translation
- **WSL**: Minimal overhead for native Linux binaries
- **Darling**: Higher overhead for framework emulation
- **ATL**: Variable overhead depending on Android APIs used

### Optimization Tips

1. **Use native binaries when possible**: Linux ELF binaries via WSL have minimal overhead
2. **Configure Wine properly**: Disable unnecessary features
3. **Limit debug output**: Disable verbose logging in production
4. **Use appropriate subsystem**: Choose the most efficient compatibility layer

## Security Considerations

### Sandboxing

Each subsystem provides different levels of sandboxing:

- **Wine**: Limited sandboxing, runs with user privileges
- **WSL**: Namespace isolation available
- **Darling**: Framework-level isolation
- **ATL**: Android permission model

### File System Access

- Applications have access to user's home directory by default
- WSL provides path translation but no additional restrictions
- Consider using containers for additional isolation

## References

- [Wine Project](https://www.winehq.org/) - Windows compatibility layer
- [Darling Project](https://github.com/darlinghq/darling) - macOS compatibility layer
- [Microsoft WSL](https://github.com/microsoft/WSL) - Windows Subsystem for Linux
- [ATL Project](https://gitlab.com/android_translation_layer/android_translation_layer) - Android Translation Layer

## Contributing

Contributions are welcome! Please see the main project documentation for contribution guidelines.

### Development Setup

1. Clone the repository
2. Install development dependencies
3. Build with debug symbols: `./configure --enable-debug && make`
4. Run tests: `make check`
5. Submit pull requests with proper documentation

## License

Goliath is released under the same license as Wine (LGPL). See the LICENSE file for details.
