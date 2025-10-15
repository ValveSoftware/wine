#!/usr/bin/env python3

# Read the configure.ac file in binary mode to preserve exact content
with open('configure.ac', 'rb') as f:
    content = f.read()

# Check if it ends with a newline
if not content.endswith(b'\n'):
    print("File does not end with newline. Adding one...")
    # Add newline
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Newline added successfully!")
else:
    print("File already ends with newline.")

# Verify the fix
with open('configure.ac', 'rb') as f:
    content = f.read()
    print(f"File now ends with newline: {content.endswith(b'\\n')}")