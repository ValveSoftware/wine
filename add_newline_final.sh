#!/bin/bash

# Check if configure.ac ends with a newline
if [[ $(tail -c1 configure.ac | wc -l) -eq 0 ]]; then
    echo "File does not end with newline. Adding one..."
    echo "" >> configure.ac
    echo "Newline added successfully."
else
    echo "File already ends with newline."
fi

# Verify the fix
echo "Checking file ending:"
tail -c10 configure.ac | od -c