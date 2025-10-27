#!/usr/bin/env python3

# Simple fix for configure.ac newline issue
with open('configure.ac', 'rb') as f:
    content = f.read()

if not content.endswith(b'\n'):
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Added newline to configure.ac")
else:
    print("File already has newline")