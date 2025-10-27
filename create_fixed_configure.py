#!/usr/bin/env python3

import shutil
import os

# Read the current configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 20 bytes: {repr(content[-20:])}")

# Create backup
backup_name = f'configure.ac.backup_{os.getpid()}'
shutil.copy2('configure.ac', backup_name)
print(f"Created backup: {backup_name}")

# Fix the content if needed
if not content.endswith(b'\\n'):
    print("Adding missing newline...")
    fixed_content = content + b'\\n'
    
    # Write the fixed content
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print("Fixed configure.ac written")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"After fix:")
    print(f"  File size: {len(verify_content)} bytes")
    print(f"  Ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"  Last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n'):
        print("SUCCESS: configure.ac now ends with newline!")
        
        # Clean autom4te cache
        if os.path.exists('autom4te.cache'):
            shutil.rmtree('autom4te.cache')
            print("Cleaned autom4te.cache")
            
        print("\\nYou can now run ./autogen.sh to test the fix")
    else:
        print("ERROR: Fix verification failed")
else:
    print("File already ends with newline - no fix needed")