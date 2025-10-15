#!/usr/bin/env python3
import shutil

# Create backup
shutil.copy('configure.ac', 'configure.ac.backup')

# Read the entire file
with open('configure.ac', 'r') as f:
    content = f.read()

print(f"Original file length: {len(content)} characters")
print(f"Ends with newline: {content.endswith('\\n')}")

if not content.endswith('\\n'):
    print("Adding newline...")
    # Write back with newline
    with open('configure.ac', 'w') as f:
        f.write(content + '\\n')
    print("Newline added!")
    
    # Verify
    with open('configure.ac', 'r') as f:
        new_content = f.read()
    print(f"New file length: {len(new_content)} characters")
    print(f"Now ends with newline: {new_content.endswith('\\n')}")
else:
    print("File already ends with newline")