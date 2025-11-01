#!/usr/bin/env python3

import shutil

# Create backup first
shutil.copy2('configure.ac', 'configure.ac.backup_original')
print("Created backup: configure.ac.backup_original")

# Read the file in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\\n'):
    print("Adding newline to end of file...")
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
else:
    print("File already ends with newline - no fix needed.")