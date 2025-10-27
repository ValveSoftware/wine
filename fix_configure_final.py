#!/usr/bin/env python3

# Fix the configure.ac newline issue
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with newline, add one
if not content.endswith(b'\n'):
    print("Adding newline to configure.ac...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    print("Fix applied successfully!")
else:
    print("File already ends with newline - no fix needed")