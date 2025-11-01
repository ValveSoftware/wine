#!/usr/bin/env python3
import os

# Check the exact ending of configure.ac
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()
    
print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {content[-20:]}")
print(f"Last 20 bytes as hex: {content[-20:].hex()}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show the last few characters
last_chars = content[-10:].decode('utf-8', errors='replace')
print(f"Last 10 characters: {repr(last_chars)}")