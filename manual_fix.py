#!/usr/bin/env python3
import shutil

# Create backup
shutil.copy('configure.ac', 'configure.ac.backup')

# Read the file
with open('configure.ac', 'r') as f:
    content = f.read()

# Check if it ends with newline
if not content.endswith('\n'):
    print("File does not end with newline. Adding one...")
    # Write back with newline
    with open('configure.ac', 'w') as f:
        f.write(content + '\n')
    print("Newline added successfully!")
else:
    print("File already ends with newline.")

print("Done!")