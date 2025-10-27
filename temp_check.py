#!/usr/bin/env python3

# Check the exact ending of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 10 bytes: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 5 bytes in hex: {content[-5:].hex()}")

# Add newline if missing
if not content.endswith(b'\n'):
    print("Adding newline...")
    with open('configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Done!")
else:
    print("Already has newline")