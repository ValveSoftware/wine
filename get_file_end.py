#!/usr/bin/env python3

# Get the exact end of configure.ac for debugging
with open('configure.ac', 'rb') as f:
    content = f.read()

# Show last 100 bytes
print("Last 100 bytes:")
print(repr(content[-100:]))

# Show as hex
print("\nLast 20 bytes as hex:")
print(' '.join(f'{b:02x}' for b in content[-20:]))

# Show the issue
print(f"\nFile ends with newline: {content.endswith(b'\\n')}")
if content:
    print(f"Last character: 0x{content[-1]:02x} ('{chr(content[-1]) if 32 <= content[-1] <= 126 else '?'}')")

# Show what we need to add
if not content.endswith(b'\n'):
    print("\nNeed to add: 0x0a (newline)")
    fixed = content + b'\n'
    print(f"Fixed size would be: {len(fixed)} bytes")