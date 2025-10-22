#!/usr/bin/env python3

# Check if configure.ac ends with newline
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show the last few characters in hex
print(f"Last 5 bytes in hex: {content[-5:].hex()}")