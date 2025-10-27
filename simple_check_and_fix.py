#!/usr/bin/env python3

# Simple check and fix for configure.ac newline issue
import os

print("Checking configure.ac newline status...")

# Read file in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 15 bytes: {repr(content[-15:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, fix it
if not content.endswith(b'\\n'):
    print("\\nFIXING: File does not end with newline")
    
    # Create backup
    with open('configure.ac.backup_final', 'wb') as f:
        f.write(content)
    print("Created backup: configure.ac.backup_final")
    
    # Add newline
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    
    print("Added newline to configure.ac")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"After fix - size: {len(new_content)} bytes")
    print(f"After fix - ends with newline: {new_content.endswith(b'\\n')}")
    
    if new_content.endswith(b'\\n'):
        print("SUCCESS: Fix applied!")
        
        # Clean cache
        if os.path.exists('autom4te.cache'):
            import shutil
            shutil.rmtree('autom4te.cache')
            print("Cleaned autom4te.cache")
    else:
        print("ERROR: Fix failed")
else:
    print("File already ends with newline - no fix needed")