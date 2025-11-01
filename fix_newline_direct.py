#!/usr/bin/env python3

import os

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"File ends with newline: {content.endswith(b'\\n')}")

# Check if we need to add a newline
if not content.endswith(b'\\n'):
    print("Adding newline to end of file...")
    
    # Create backup
    with open('configure.ac.backup_before_newline_fix', 'wb') as f:
        f.write(content)
    print("Created backup: configure.ac.backup_before_newline_fix")
    
    # Add newline and write back
    new_content = content + b'\\n'
    with open('configure.ac', 'wb') as f:
        f.write(new_content)
    
    print(f"New file size: {len(new_content)} bytes")
    print("✓ Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    print(f"Verification - ends with newline: {verify_content.endswith(b'\\n')}")
    
else:
    print("✓ File already ends with newline - no fix needed!")

print("\\nDone! You can now run ./autogen.sh")