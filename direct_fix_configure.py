#!/usr/bin/env python3

# Read the current configure.ac file in binary mode
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\\n'):
    print("File does not end with newline. Adding one...")
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    print("Newline added successfully!")
    
    # Verify the fix
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already ends with newline - no changes needed")

# Clean autom4te cache if it exists
import os
import shutil
if os.path.exists('/workspace/autom4te.cache'):
    shutil.rmtree('/workspace/autom4te.cache')
    print("Removed autom4te.cache directory")