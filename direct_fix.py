#!/usr/bin/env python3

# Direct fix for configure.ac newline issue
print("Fixing configure.ac newline issue...")

# Read the file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Check if fix is needed
if not content.endswith(b'\n'):
    print("Adding newline...")
    # Add newline
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    print("Fix applied successfully!")
else:
    print("File already ends with newline - no fix needed")

# Clean cache
import os
if os.path.exists('autom4te.cache'):
    import shutil
    shutil.rmtree('autom4te.cache')
    print("Cleaned autom4te.cache")

print("Done!")