#!/usr/bin/env python3

# Read the current configure.ac file
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\\n'):
    print("Adding newline to configure.ac...")
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    print("Newline added!")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    print("Fix complete!")
else:
    print("File already ends with newline")