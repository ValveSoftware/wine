#!/bin/bash
cd /workspace

# First, let's check the current state
echo "=== Current state ==="
echo "File size: $(wc -c < configure.ac) bytes"
echo "Last character (hex): $(tail -c1 configure.ac | od -tx1 -An | tr -d ' ')"
echo "Ends with newline: $(if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then echo "yes"; else echo "no"; fi)"

# Apply the fix
echo -e "\n=== Applying fix ==="
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "Adding newline to configure.ac..."
    echo "" >> configure.ac
    echo "Newline added."
else
    echo "File already ends with newline."
fi

# Clean cache
echo -e "\n=== Cleaning cache ==="
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "Removed autom4te.cache"
else
    echo "No cache to remove"
fi

# Verify the fix
echo -e "\n=== Verification ==="
echo "New file size: $(wc -c < configure.ac) bytes"
echo "Last character (hex): $(tail -c1 configure.ac | od -tx1 -An | tr -d ' ')"
echo "Now ends with newline: $(if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then echo "yes"; else echo "no"; fi)"

echo -e "\n=== Last few lines ==="
tail -3 configure.ac | cat -n