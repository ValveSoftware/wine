# XeSS Integration for Proton Wine

This implementation provides a Wine wrapper for Intel's XeSS (Xe Super Sampling) SR API that redirects calls to an alternate implementation.

## Overview

XeSS is Intel's AI-based temporal super sampling and anti-aliasing technology. This Wine DLL intercepts XeSS API calls from Windows games and redirects them to an alternate implementation.

## Rationale

Where possible, XeSS uses Intel's [XMX](https://www.intel.com/content/www/us/en/support/articles/000091112/graphics.html). For environments where XMX is not available, the XeSS library falls back to a slower path.

All XeSS titles running through Proton must currently take the slow path because the current fast path implementation requires libraries that are not available in Intel's open-source Vulkan driver. This DLL forwards XeSS API calls to an implementation that may have access to XMX optimizations, thereby enabling fast XMX-enabled XeSS in Proton.

## Building

The libxess.dll will be built along with other Wine DLLs.

## Usage

### Prerequisites
An XeSS shared object implementation. Set the path using an environment variable:

```bash
export XESS_LIB_OVERRIDE=/path/to/libxess.so
```

If not set, the wrapper will look for `libxess_override.so` in the library search path.

### Running Games

Simply run the game through Proton/Wine. If the game uses XeSS, the calls will be automatically intercepted and redirected.

```bash
wine YourGame.exe
```

Or through Proton:
```bash
XESS_LIB_OVERRIDE=/path/to/libxess.so %command%
```

### Debug Output

Enable Wine debug output to see XeSS interception:

```bash
WINEDEBUG=+xess wine YourGame.exe
```

## License

This implementation is licensed under Wine's LGPL 2.1 license.

## References

- [Intel XeSS SDK](https://github.com/intel/xess)
- [XeSS Developer Guide](https://github.com/intel/xess/blob/main/doc/xess_sr_developer_guide_english.md)
- [Wine Documentation](https://wiki.winehq.org/)
