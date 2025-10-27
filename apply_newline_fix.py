#!/usr/bin/env python3

# Apply newline fix to configure.ac
import os

# First, check current state
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Before fix:")
print(f"  File size: {len(content)} bytes")
print(f"  Last 20 bytes: {repr(content[-20:])}")
print(f"  Ends with newline: {content.endswith(b'\\n')}")

# Apply fix if needed
if not content.endswith(b'\n'):
    # Create backup first
    with open('configure.ac.backup_before_newline_fix', 'wb') as f:
        f.write(content)
    print("Created backup: configure.ac.backup_before_newline_fix")
    
    # Apply fix
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Added newline to configure.ac")
    
    # Verify fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"After fix:")
    print(f"  File size: {len(new_content)} bytes")
    print(f"  Last 20 bytes: {repr(new_content[-20:])}")
    print(f"  Ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("File already has newline - no fix needed")