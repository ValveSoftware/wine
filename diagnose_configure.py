#!/usr/bin/env python3

# Diagnostic script to check configure.ac ending
import os

# Read the file in binary mode to see exact bytes
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline (LF): {content.endswith(b'\\n')}")
print(f"Ends with CRLF: {content.endswith(b'\\r\\n')}")
print(f"Ends with CR: {content.endswith(b'\\r')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()}, decimal: {ord(last_char)})")
    
    # Show last few characters for context
    print(f"Last 10 characters: {repr(content[-10:])}")

# Also check in text mode to see how Python interprets it
with open('/workspace/configure.ac', 'r') as f:
    text_content = f.read()

print(f"\\nText mode analysis:")
print(f"Text ends with newline: {text_content.endswith('\\n')}")
if text_content:
    print(f"Text last character: {repr(text_content[-1])}")

# Check if the issue is specifically at line 1394
lines = text_content.split('\\n')
print(f"Total lines when split by \\n: {len(lines)}")
print(f"Last 3 lines:")
for i, line in enumerate(lines[-3:], start=len(lines)-2):
    print(f"  Line {i}: {repr(line)}")