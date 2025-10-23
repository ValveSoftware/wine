#!/usr/bin/env python3

import os
import subprocess

print("=== Fixing configure.ac newline issue ===")

# Check if configure.ac ends with newline
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show the last few characters in hex
print(f"Last 5 bytes in hex: {content[-5:].hex()}")

# If it doesn't end with newline, fix it
if not content.endswith(b'\n'):
    print("\\nFile does not end with newline. Adding one...")
    with open('configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("\\nFile already ends with newline - no changes needed")

# Clean autom4te cache
print("\\n=== Cleaning autom4te cache ===")
try:
    if os.path.exists('autom4te.cache'):
        subprocess.run(['rm', '-rf', 'autom4te.cache'], check=True)
        print("autom4te.cache removed successfully")
    else:
        print("autom4te.cache does not exist")
except Exception as e:
    print(f"Error removing cache: {e}")

print("\\n=== Fix completed ===")