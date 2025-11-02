#!/usr/bin/env python3

# Quick check of configure.ac ending
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1]
    print(f"Last character: ASCII {last_char} = {repr(chr(last_char))}")
    
    if last_char == 10:  # \n
        print("✓ File ends with newline")
    else:
        print("✗ File does NOT end with newline")
        print("NEEDS FIX: Add newline character")