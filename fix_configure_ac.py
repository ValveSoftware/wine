#!/usr/bin/env python3

import os

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("File does not end with newline. Adding one...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already ends with newline")

# Clean autom4te cache if it exists
if os.path.exists('autom4te.cache'):
    import shutil
    shutil.rmtree('autom4te.cache')
    print("autom4te.cache removed")

print("Fix completed!")