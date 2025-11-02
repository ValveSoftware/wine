#!/usr/bin/env python3

# Check current state of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 10 bytes: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\n')}")

if content:
    last_char = content[-1]
    print(f"Last character: ASCII {last_char} ({repr(chr(last_char))})")