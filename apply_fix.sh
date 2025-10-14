#!/bin/bash

echo "Checking if configure.ac ends with newline..."

# Check if file ends with newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with newline. Adding one..."
    # Use printf to add newline
    printf '\n' >> configure.ac
    echo "Newline added successfully."
else
    echo "File already ends with newline."
fi

# Verify
echo "Verification:"
if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
    echo "SUCCESS: File now ends with newline"
else
    echo "ERROR: File still doesn't end with newline"
fi