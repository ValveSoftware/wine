# Goliath WSL Examples

This directory contains examples demonstrating how to use the Goliath WSL compatibility layer.

## Files

- `wsl_example.c` - C program demonstrating WSL API usage
- `README.md` - This file

## Building the Example

To build the WSL example program:

```bash
# Make sure Goliath is built first
cd /path/to/goliath
./configure
make

# Build the example
cd examples
gcc -o wsl_example wsl_example.c -I../include -L../dlls/wsl -lwsl
```

## Running the Example

```bash
# Basic usage (shows WSL initialization and configuration)
./wsl_example

# Launch a Linux application through WSL
./wsl_example /bin/echo "Hello from Goliath WSL!"
./wsl_example /usr/bin/whoami
./wsl_example /bin/ls -la /tmp
```

## Using Goliath WSL

### Command Line Usage

The easiest way to use Goliath WSL is through the unified launcher:

```bash
# From the Goliath root directory
./goliath-launch.sh /bin/echo "Hello World!"
./goliath-launch.sh /usr/bin/ls -la
./goliath-launch.sh /usr/bin/python3 myscript.py
```

### Programmatic Usage

You can also use the WSL API directly in your C programs:

```c
#include "wsl.h"

int main() {
    // Initialize WSL
    wsl_init_config();
    wsl_setup_environment();
    
    // Launch a Linux process
    char *argv[] = {"/bin/echo", "Hello WSL!", NULL};
    pid_t pid = wsl_launch_process("/bin/echo", argv, NULL);
    int status = wsl_wait_for_process(pid);
    
    // Cleanup
    wsl_cleanup();
    return status;
}
```

## Configuration

WSL can be configured through:

1. **Environment variables**:
   ```bash
   export WSL_DISTRO_NAME="MyDistro"
   export WSL_ENABLE_INTEROP=1
   ```

2. **Configuration file** (`~/.config/goliath/wsl.conf`):
   ```ini
   [wsl]
   distro_name = MyDistro
   enable_interop = true
   enable_drive_mounting = true
   ```

See `../dlls/wsl/wsl.conf.example` for a complete configuration example.

## Features Demonstrated

The examples show:

- WSL initialization and configuration
- Environment variable setup
- Path conversion (Windows ↔ Linux)
- Process launching and management
- Signal handling
- Filesystem structure creation
- Configuration management

## Troubleshooting

If you encounter issues:

1. **Library not found**: Make sure Goliath is properly built and installed
2. **Permission errors**: Ensure the Linux binaries you're trying to run are executable
3. **Path issues**: Check that paths are correctly converted using `wsl_convert_path()`
4. **Environment problems**: Verify WSL environment setup with debug output

Enable debug mode:
```bash
export GOLIATH_DEBUG=1
./goliath-launch.sh your_app
```