#!/usr/bin/env python3

# Simple check of configure.ac ending
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()
    
print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Also check text mode
with open('/workspace/configure.ac', 'r') as f:
    text_content = f.read()
    
print(f"Text mode - ends with newline: {text_content.endswith('\\n')}")
if text_content:
    print(f"Text mode - last character: {repr(text_content[-1])}")