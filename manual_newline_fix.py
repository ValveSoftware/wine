#!/usr/bin/env python3

import os

# Read current content
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 20 bytes: {repr(content[-20:])}")

# If it doesn't end with newline, add one
if not content.endswith(b'\\n'):
    print("Adding newline...")
    
    # Create backup
    backup_name = 'configure.ac.backup_manual_fix'
    with open(backup_name, 'wb') as f:
        f.write(content)
    print(f"Created backup: {backup_name}")
    
    # Add newline and write back
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    print(f"Last 20 bytes: {repr(new_content[-20:])}")
    
else:
    print("File already ends with newline")