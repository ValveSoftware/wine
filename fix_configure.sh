#!/bin/bash

# Backup the original file
cp configure.ac configure_backup.ac

# Add a newline to the end of configure.ac if it doesn't have one
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "Adding newline to configure.ac..."
    echo "" >> configure.ac
    echo "Newline added successfully."
else
    echo "File already ends with a newline."
fi

# Verify the fix
echo "Last few lines of configure.ac:"
tail -3 configure.ac | cat -n
echo "--- End of file ---"

# Check if the file now ends with a newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
    echo "SUCCESS: File now ends with a newline."
else
    echo "ERROR: File still doesn't end with a newline."
fi