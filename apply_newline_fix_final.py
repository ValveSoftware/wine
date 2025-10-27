#!/usr/bin/env python3

# Apply the newline fix to configure.ac
import os

# Read the current content
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 20 bytes: {repr(content[-20:])}")

# Check if we need to add a newline
if not content.endswith(b'\\n'):
    print("File does not end with newline - adding one...")
    
    # Create backup
    backup_name = 'configure.ac.backup_final_fix'
    with open(backup_name, 'wb') as f:
        f.write(content)
    print(f"Created backup: {backup_name}")
    
    # Add newline and write back
    new_content = content + b'\\n'
    with open('configure.ac', 'wb') as f:
        f.write(new_content)
    
    print("Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"After fix:")
    print(f"  File size: {len(verify_content)} bytes")
    print(f"  Ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"  Last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n'):
        print("SUCCESS: configure.ac now ends with newline!")
    else:
        print("ERROR: Fix failed")
        
else:
    print("File already ends with newline - no fix needed")

# Clean autom4te cache if it exists
if os.path.exists('autom4te.cache'):
    import shutil
    shutil.rmtree('autom4te.cache')
    print("Cleaned autom4te.cache directory")