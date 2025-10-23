#!/bin/bash

# Check if configure.ac ends with newline
if [[ $(tail -c1 configure.ac | wc -l) -eq 0 ]]; then
    echo "File does NOT end with newline, adding one..."
    echo "" >> configure.ac
    echo "Newline added successfully!"
else
    echo "File already ends with newline"
fi

# Clean cache
rm -rf autom4te.cache 2>/dev/null || true
echo "Cache cleaned"