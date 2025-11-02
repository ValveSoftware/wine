#!/bin/bash

echo "Checking if configure.ac ends with newline..."

# Check if the last character is a newline
if [[ $(tail -c1 configure.ac | wc -l) -eq 0 ]]; then
    echo "File does NOT end with newline. Adding one..."
    
    # Create backup
    cp configure.ac configure.ac.backup_$(date +%Y%m%d_%H%M%S)
    
    # Add newline
    echo "" >> configure.ac
    
    echo "Newline added successfully!"
    
    # Verify
    if [[ $(tail -c1 configure.ac | wc -l) -eq 1 ]]; then
        echo "Verification: File now ends with newline ✓"
    else
        echo "Verification: Something went wrong ✗"
    fi
else
    echo "File already ends with newline ✓"
fi

echo "Testing autoreconf..."
autoreconf --warnings=all 2>&1 | head -10