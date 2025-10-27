#!/usr/bin/env python3

import os
import shutil

print("Executing configure.ac newline fix...")

# Read current file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if not content.endswith(b'\\n'):
    print("File does not end with newline - applying fix...")
    
    # Create backup
    backup_name = f'configure.ac.backup_{os.getpid()}'
    with open(backup_name, 'wb') as f:
        f.write(content)
    print(f"Created backup: {backup_name}")
    
    # Add newline
    fixed_content = content + b'\\n'
    
    # Write back
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"After fix - size: {len(verify_content)} bytes")
    print(f"After fix - ends with newline: {verify_content.endswith(b'\\n')}")
    
    if verify_content.endswith(b'\\n'):
        print("SUCCESS: configure.ac now ends with newline!")
        
        # Clean autom4te cache
        if os.path.exists('autom4te.cache'):
            shutil.rmtree('autom4te.cache')
            print("Cleaned autom4te.cache")
            
        print("Fix complete! The autoreconf issue should now be resolved.")
    else:
        print("ERROR: Fix verification failed")
else:
    print("File already ends with newline - no fix needed")

print("Done.")