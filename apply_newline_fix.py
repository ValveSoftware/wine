#!/usr/bin/env python3

import os
import shutil

# Read the current file
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("Adding newline...")
    with open('/workspace/configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Newline added!")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already ends with newline")

# Clean cache
cache_dir = '/workspace/autom4te.cache'
if os.path.exists(cache_dir):
    shutil.rmtree(cache_dir)
    print("Removed autom4te.cache")