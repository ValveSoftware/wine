#!/bin/bash

echo "=== Cleaning autotools cache and fixing configure.ac ==="

# Step 1: Clean the autotools cache
echo "Removing autom4te.cache directory..."
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "autom4te.cache removed"
else
    echo "autom4te.cache not found"
fi

# Step 2: Fix the newline issue in configure.ac
echo ""
echo "Fixing configure.ac newline issue..."

# Check if the file ends with a newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with a newline. Adding one..."
    
    # Use printf to append newline (most reliable method)
    printf '\n' >> configure.ac
    
    # Verify the fix
    if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
        echo "SUCCESS: Newline added to configure.ac"
    else
        echo "ERROR: Failed to add newline"
        exit 1
    fi
else
    echo "File already ends with a newline."
fi

# Step 3: Verify the fix
echo ""
echo "=== Verification ==="
echo "Last 3 lines of configure.ac:"
tail -3 configure.ac | cat -n
echo "--- End of file ---"

echo ""
echo "Checking file ending:"
if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
    echo "✓ File now ends with a proper newline"
else
    echo "✗ File still doesn't end with a newline"
    exit 1
fi

echo ""
echo "=== Ready for autoreconf ==="
echo "The configure.ac file has been fixed and cache cleared."
echo "You can now run: autoreconf --force --install"