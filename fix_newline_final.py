#!/usr/bin/env python3

import os
import shutil

# Create backup first
shutil.copy2('configure.ac', 'configure.ac.backup_final')
print("Created backup: configure.ac.backup_final")

# Read the file in binary mode to check exact ending
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 10 bytes: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("Adding newline character...")
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