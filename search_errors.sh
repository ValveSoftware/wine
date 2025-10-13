#!/bin/bash
# Search for lines that might cause ": command not found" error
echo "Searching for empty commands or standalone colons..."
grep -n "^[[:space:]]*:$" /workspace/configure.ac
echo "Searching for lines with just colons and whitespace..."
grep -n "^[[:space:]]*:[[:space:]]*$" /workspace/configure.ac
echo "Searching for incomplete variable assignments..."
grep -n "^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*=$" /workspace/configure.ac