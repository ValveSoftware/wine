#!/usr/bin/env python3

import os
import shutil

# Create backup first
shutil.copy2('configure.ac', 'configure.ac.backup_original')
print("Created backup: configure.ac.backup_original")

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
    
    if new_content.endswith(b'\\n') and len(new_content) == len(content) + 1:
        print("SUCCESS: Fix verified!")
    else:
        print("ERROR: Fix verification failed!")
        # Restore backup
        shutil.copy2('configure.ac.backup_original', 'configure.ac')
        print("Restored from backup")
        exit(1)
        
else:
    print("File already ends with newline - no fix needed")

# Clean autom4te cache
if os.path.exists('autom4te.cache'):
    shutil.rmtree('autom4te.cache')
    print("Cleaned autom4te.cache")

print("Fix completed successfully!")