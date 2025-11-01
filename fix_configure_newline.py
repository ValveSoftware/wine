#!/usr/bin/env python3

import os
import shutil

filename = 'configure.ac'

print(f"Checking {filename} for newline ending...")

# Read the file in binary mode to preserve exact content
with open(filename, 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")

# Check if file ends with newline
if content.endswith(b'\n'):
    print("✓ File already ends with newline - no fix needed!")
else:
    print("✗ File does NOT end with newline - applying fix...")
    
    # Create backup
    backup_name = f"{filename}.backup_newline_fix"
    shutil.copy2(filename, backup_name)
    print(f"Created backup: {backup_name}")
    
    # Add newline
    with open(filename, 'ab') as f:
        f.write(b'\n')
    
    # Verify fix
    with open(filename, 'rb') as f:
        new_content = f.read()
    
    if new_content.endswith(b'\n') and len(new_content) == len(content) + 1:
        print("✓ SUCCESS: Newline added successfully!")
        print(f"New file size: {len(new_content)} bytes")
        print(f"Last 20 bytes: {repr(new_content[-20:])}")
    else:
        print("✗ ERROR: Fix failed!")
        # Restore backup
        shutil.copy2(backup_name, filename)
        print("Restored from backup")