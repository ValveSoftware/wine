#!/usr/bin/env python3

# Read the configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# If it doesn't end with a newline, add one
if not content.endswith(b'\n'):
    print("Adding newline to end of file...")
    content += b'\n'
    
    # Write the fixed content back
    with open('configure.ac', 'wb') as f:
        f.write(content)
    
    print("Fixed! File now ends with newline.")
else:
    print("File already ends with newline.")

# Verify the fix
with open('configure.ac', 'rb') as f:
    new_content = f.read()
    
print(f"New file size: {len(new_content)} bytes")
print(f"Now ends with newline: {new_content.endswith(b'\\n')}")