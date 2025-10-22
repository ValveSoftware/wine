#!/usr/bin/env python3

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

# Check if it ends with newline
if not content.endswith(b'\n'):
    # Append a newline
    with open('configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Newline added to configure.ac")
else:
    print("File already ends with newline")