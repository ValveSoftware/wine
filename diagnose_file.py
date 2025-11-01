#!/usr/bin/env python3

# Diagnose the exact state of configure.ac
import os

filename = 'configure.ac'

if not os.path.exists(filename):
    print(f"ERROR: {filename} does not exist!")
    exit(1)

with open(filename, 'rb') as f:
    content = f.read()

print(f"File: {filename}")
print(f"Size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Last 30 bytes as hex: {content[-30:].hex()}")
print(f"Ends with newline (\\n): {content.endswith(b'\\n')}")
print(f"Ends with carriage return + newline (\\r\\n): {content.endswith(b'\\r\\n')}")

# Check the very last character
if content:
    last_char = content[-1]
    print(f"Last character: {repr(chr(last_char))} (ASCII {last_char})")
    
    if last_char == 10:  # \n
        print("✓ File ends with Unix newline")
    elif last_char == 13:  # \r
        print("⚠ File ends with Mac classic carriage return")
    elif len(content) >= 2 and content[-2:] == b'\r\n':
        print("✓ File ends with Windows CRLF")
    else:
        print("✗ File does NOT end with any newline character")
else:
    print("ERROR: File is empty!")