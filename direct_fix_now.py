#!/usr/bin/env python3

# Read the current configure.ac file
with open('configure.ac', 'r') as f:
    content = f.read()

print(f"Original content ends with newline: {content.endswith('\\n')}")

# If it doesn't end with newline, add one
if not content.endswith('\n'):
    print("Adding newline to configure.ac...")
    with open('configure.ac', 'w') as f:
        f.write(content + '\n')
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'r') as f:
        new_content = f.read()
    print(f"Now ends with newline: {new_content.endswith('\\n')}")
else:
    print("File already ends with newline")

print("Done!")