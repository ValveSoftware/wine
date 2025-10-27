#!/usr/bin/env python3

# Examine the exact end of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Total file size: {len(content)} bytes")
print(f"Last 50 bytes: {repr(content[-50:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show the last few characters
if len(content) >= 10:
    for i in range(10):
        idx = len(content) - 10 + i
        byte_val = content[idx]
        char = chr(byte_val) if 32 <= byte_val <= 126 else f'\\x{byte_val:02x}'
        print(f"Byte {idx}: 0x{byte_val:02x} '{char}'")

# Try to fix it
if not content.endswith(b'\n'):
    print("\nAdding newline...")
    fixed_content = content + b'\n'
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    print("Fixed!")
else:
    print("\nAlready has newline")