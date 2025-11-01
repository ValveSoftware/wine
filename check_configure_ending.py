#!/usr/bin/env python3

# Check if configure.ac ends with newline
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last character: {repr(content[-1:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content.endswith(b'\\n'):
    print("✓ File already ends with newline")
else:
    print("✗ File does NOT end with newline - needs fix")
    print(f"Last 10 bytes: {repr(content[-10:])}")