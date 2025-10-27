#!/usr/bin/env python3

import shutil
import os

# Create backup
shutil.copy('configure.ac', 'configure.ac.backup_manual_fix')
print("Backup created: configure.ac.backup_manual_fix")

# Read the file in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("File does not end with newline. Adding one...")
    
    # Write the fixed content
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    
    print("Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    
    # Show the last few bytes
    print(f"Last 20 bytes: {repr(new_content[-20:])}")
else:
    print("File already ends with newline - no fix needed")

# Clean cache
if os.path.exists('autom4te.cache'):
    shutil.rmtree('autom4te.cache')
    print("autom4te.cache removed")

print("Fix completed!")