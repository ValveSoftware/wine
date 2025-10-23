#!/usr/bin/env python3

# Check the exact bytes at the end of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with \\n (0x0a): {content.endswith(b'\\n')}")

# Show the last character
if len(content) > 0:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")
    
    if last_char == b':':
        print("CONFIRMED: File ends with ':' character, missing newline!")
    elif last_char == b'\n':
        print("File correctly ends with newline")
    else:
        print(f"File ends with unexpected character: {last_char}")