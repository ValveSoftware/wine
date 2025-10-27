#!/usr/bin/env python3

# Check configure.ac structure
with open('configure.ac', 'r') as f:
    lines = f.readlines()

print(f"Total lines: {len(lines)}")
print("Last 5 lines:")
for i, line in enumerate(lines[-5:], start=len(lines)-4):
    print(f"Line {i}: {repr(line)}")

# Check in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"\\nBinary analysis:")
print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Try to fix it
if not content.endswith(b'\\n'):
    print("\\nApplying fix...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    print("Fix applied!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"After fix - ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("\\nFile already ends with newline")