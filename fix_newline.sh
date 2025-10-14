#!/bin/bash
# Add newline to configure.ac if it doesn't end with one
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "Adding newline to configure.ac..."
    printf '\n' >> configure.ac
    echo "Newline added successfully."
else
    echo "File already ends with a newline."
fi