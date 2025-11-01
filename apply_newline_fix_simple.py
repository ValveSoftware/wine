#!/usr/bin/env python3

# Simple fix for configure.ac newline issue
filename = 'configure.ac'

# Read current content
with open(filename, 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if not content.endswith(b'\\n'):
    print("Adding newline...")
    # Create backup
    with open(f"{filename}.backup_before_newline_fix", 'wb') as f:
        f.write(content)
    
    # Write content with newline
    with open(filename, 'wb') as f:
        f.write(content + b'\\n')
    
    print("✓ Newline added successfully!")
    
    # Verify
    with open(filename, 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("✓ File already ends with newline - no fix needed!")