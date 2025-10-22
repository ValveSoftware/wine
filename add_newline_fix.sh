#!/bin/bash

# Check if file ends with newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does not end with newline - adding one..."
    printf '\n' >> configure.ac
    echo "Newline added successfully!"
else
    echo "File already ends with newline - no changes needed"
fi

# Verify the fix
echo "Verification:"
echo "File now ends with newline: $(if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then echo "YES"; else echo "NO"; fi)"

# Clean cache
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "Removed autom4te.cache"
fi