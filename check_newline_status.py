#!/usr/bin/env python3

# Check if configure.ac ends with newline
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()
    
print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")