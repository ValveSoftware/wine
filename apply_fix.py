#!/usr/bin/env python3

# Read the current configure.ac file in binary mode
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 10 bytes: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("File does not end with newline. Adding one...")
    with open('/workspace/configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Newline added successfully!")
    
    # Verify the fix
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already ends with newline - no changes needed")

# Also clean up autom4te.cache if it exists
import os
import shutil
cache_dir = '/workspace/autom4te.cache'
if os.path.exists(cache_dir):
    shutil.rmtree(cache_dir)
    print("Removed autom4te.cache directory")