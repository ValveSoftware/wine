#!/usr/bin/env python3

# Simple check of configure.ac ending
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 10 bytes: {content[-10:]}")
print(f"Last 10 bytes as text: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_byte = content[-1]
    print(f"Last byte: 0x{last_byte:02x} ({chr(last_byte) if 32 <= last_byte <= 126 else 'non-printable'})")
    
    if last_byte == ord(':'):
        print("PROBLEM: File ends with colon, needs newline!")
    elif last_byte == ord('\n'):
        print("OK: File ends with newline")
    else:
        print(f"UNEXPECTED: File ends with character {chr(last_byte)}")