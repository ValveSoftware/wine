#!/usr/bin/env python3

# Immediate fix for configure.ac
print("Reading configure.ac...")

# Read the file
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Apply fix
if not content.endswith(b'\\n'):
    print("Adding newline...")
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    print("Done!")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("Already has newline")