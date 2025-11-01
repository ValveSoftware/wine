#!/usr/bin/env python3

# Read the current file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\n')}")

# Add newline if missing
if not content.endswith(b'\n'):
    print("Adding newline...")
    content += b'\n'
    
    # Write back the fixed content
    with open('configure.ac', 'wb') as f:
        f.write(content)
    
    print(f"New file size: {len(content)} bytes")
    print("✓ Newline added successfully!")
else:
    print("File already ends with newline")