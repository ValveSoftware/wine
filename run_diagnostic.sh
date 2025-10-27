#!/bin/bash

echo "Running diagnostic on configure.ac..."

# Check if file ends with newline using shell commands
echo "Shell check - does file end with newline:"
if [ "$(tail -c1 /workspace/configure.ac | wc -l)" -eq 0 ]; then
    echo "NO - file does not end with newline"
else
    echo "YES - file ends with newline"
fi

echo ""
echo "Last 20 bytes in hex:"
tail -c 20 /workspace/configure.ac | hexdump -C

echo ""
echo "Last 10 characters:"
tail -c 10 /workspace/configure.ac | od -c