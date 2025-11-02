#!/usr/bin/env python3

import os

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"File ends with newline: {content.endswith(b'\\n')}")

# Check the last few bytes
if len(content) >= 10:
    print(f"Last 10 bytes: {repr(content[-10:])}")

# If it doesn't end with newline, add one
if not content.endswith(b'\\n'):
    print("Adding newline to end of file...")
    
    # Create backup first
    with open('configure.ac.backup_before_newline_fix', 'wb') as f:
        f.write(content)
    print("Created backup: configure.ac.backup_before_newline_fix")
    
    # Add newline
    new_content = content + b'\\n'
    
    # Write back to file
    with open('configure.ac', 'wb') as f:
        f.write(new_content)
    
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    print(f"New file size: {len(verify_content)} bytes")
    print(f"Now ends with newline: {verify_content.endswith(b'\\n')}")
    
    if verify_content.endswith(b'\\n') and len(verify_content) == len(content) + 1:
        print("✓ SUCCESS: Fix applied correctly!")
    else:
        print("✗ ERROR: Fix verification failed!")
        # Restore backup
        with open('configure.ac.backup_before_newline_fix', 'rb') as f:
            original = f.read()
        with open('configure.ac', 'wb') as f:
            f.write(original)
        print("Restored from backup")
else:
    print("✓ File already ends with newline - no fix needed.")