#!/usr/bin/env python3

# Direct fix for configure.ac newline issue
print("Applying newline fix to configure.ac...")

# Read current content
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Check if fix is needed
if not content.endswith(b'\\n'):
    print("File does not end with newline - applying fix...")
    
    # Add newline and write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    
    print("Newline added successfully!")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    
    if new_content.endswith(b'\\n'):
        print("✓ SUCCESS: configure.ac now ends with newline!")
    else:
        print("✗ ERROR: Fix failed")
else:
    print("File already ends with newline - no fix needed")

# Clean autom4te cache
import os
import shutil
if os.path.exists('/workspace/autom4te.cache'):
    shutil.rmtree('/workspace/autom4te.cache')
    print("Cleaned autom4te.cache directory")

print("Fix complete!")