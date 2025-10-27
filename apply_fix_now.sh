#!/bin/bash

echo "Checking configure.ac ending..."

# Check current state
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does not end with newline - fixing..."
    
    # Create backup
    cp configure.ac configure.ac.backup_$(date +%s)
    echo "Created backup"
    
    # Add newline
    printf '\n' >> configure.ac
    echo "Added newline to configure.ac"
    
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

# Clean autom4te cache to ensure fresh build
if [ -d "autom4te.cache" ]; then
    rm -rf autom4te.cache
    echo "Cleaned autom4te.cache"
fi

echo "Fix complete. You can now run ./autogen.sh"