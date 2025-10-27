#!/usr/bin/env python3

# Quick check of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 10 bytes: {repr(content[-10:])}")

if content:
    last_byte = content[-1:]
    print(f"Last byte: {repr(last_byte)} (decimal: {ord(last_byte)})")
    
    # Check if it's a newline (ASCII 10)
    if ord(last_byte) == 10:
        print("Last byte is a newline character")
    else:
        print("Last byte is NOT a newline character")