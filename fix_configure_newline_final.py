#!/usr/bin/env python3

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Check if it ends with newline, if not, add one
if not content.endswith(b'\n'):
    print("Adding newline to configure.ac...")
    # Append a newline
    with open('configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already ends with newline - no changes needed")