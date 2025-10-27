#!/bin/bash

# Simple fix for configure.ac newline issue
echo "Checking if configure.ac ends with newline..."

# Check if file ends with newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with newline - fixing..."
    
    # Add newline using printf (more reliable than echo)
    printf '\n' >> configure.ac
    echo "Newline added to configure.ac"
    
    # Verify
    if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
        echo "SUCCESS: File now ends with newline"
    else
        echo "ERROR: Fix failed"
        exit 1
    fi
else
    echo "File already ends with newline"
fi

# Clean autom4te cache
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "Cleaned autom4te.cache"
fi

echo "Fix complete!"