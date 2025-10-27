#!/bin/bash

echo "Fixing configure.ac newline issue..."

# Check current state
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with newline - applying fix..."
    
    # Create backup
    cp configure.ac configure.ac.backup_$(date +%s)
    echo "Created backup"
    
    # Add newline using echo
    echo "" >> configure.ac
    echo "Added newline to configure.ac"
    
    # Verify fix
    if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
        echo "SUCCESS: File now ends with newline"
        
        # Clean autom4te cache
        if [ -d "autom4te.cache" ]; then
            rm -rf autom4te.cache
            echo "Cleaned autom4te.cache"
        fi
        
        echo "Fix complete!"
    else
        echo "ERROR: Fix failed"
        exit 1
    fi
else
    echo "File already ends with newline - no fix needed"
fi