#!/usr/bin/env python3

import os
import shutil

print("Checking configure.ac for newline issue...")

# Read the file in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show last few bytes for debugging
print(f"Last 20 bytes: {repr(content[-20:])}")

# Fix if needed
if not content.endswith(b'\n'):
    print("Adding newline to configure.ac...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    print("SUCCESS: Newline added to configure.ac")
else:
    print("File already ends with newline")

# Clean cache
if os.path.exists('autom4te.cache'):
    shutil.rmtree('autom4te.cache')
    print("Removed autom4te.cache")

print("Fix completed!")