#!/usr/bin/env python3

# Direct fix for configure.ac newline issue
import os

# Read the file in binary mode to preserve exact content
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Check if we need to add newline
if not content.endswith(b'\n'):
    print("File does not end with newline - adding one")
    
    # Add newline
    new_content = content + b'\n'
    
    # Write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(new_content)
    
    print(f"Fixed! New file size: {len(new_content)} bytes")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"Verification - ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"Last 30 bytes: {repr(verify_content[-30:])}")
    
else:
    print("File already ends with newline - no fix needed")