#!/usr/bin/env python3

# Fix the newline issue in configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

# Check if file ends with newline
if not content.endswith(b'\n'):
    print("File does not end with newline, adding one...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Newline added successfully!")
else:
    print("File already ends with newline")