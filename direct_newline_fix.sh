#!/bin/bash

echo "=== Fixing configure.ac newline issue ==="

# Backup the original file
cp configure.ac configure.ac.backup_$(date +%Y%m%d_%H%M%S)
echo "Created backup"

# Check if file ends with newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does not end with newline - fixing..."
    
    # Add newline using printf (more reliable than echo)
    printf '\n' >> configure.ac
    
    echo "Newline added successfully"
else
    echo "File already ends with newline"
fi

# Remove autom4te cache if it exists
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "Removed autom4te.cache directory"
fi

# Verify the fix
echo "=== Verification ==="
echo "File now ends with newline: $(if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then echo "YES"; else echo "NO"; fi)"

echo "Last 3 lines of configure.ac:"
tail -3 configure.ac | cat -n

echo "=== Fix complete ==="