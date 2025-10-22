#!/usr/bin/env python3
# Simple fix for configure.ac newline issue
with open('configure.ac', 'rb') as f:
    data = f.read()
if not data.endswith(b'\n'):
    with open('configure.ac', 'ab') as f:
        f.write(b'\n')
    print("Added newline to configure.ac")
else:
    print("File already ends with newline")