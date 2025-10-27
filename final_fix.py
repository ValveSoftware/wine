#!/usr/bin/env python3

# Read the entire configure.ac file
with open('/workspace/configure.ac', 'r') as f:
    content = f.read()

# Check if it ends with newline
if not content.endswith('\n'):
    print("Adding newline to configure.ac")
    # Write back with newline
    with open('/workspace/configure.ac', 'w') as f:
        f.write(content + '\n')
    print("Newline added successfully!")
else:
    print("File already ends with newline")

# Verify
with open('/workspace/configure.ac', 'r') as f:
    new_content = f.read()

print(f"File now ends with newline: {new_content.endswith('\\n')}")