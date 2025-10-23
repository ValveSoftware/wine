#!/bin/bash

# Check current state
echo "Checking current file ending..."
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does not end with newline. Adding one..."
    echo "" >> configure.ac
    echo "Newline added successfully."
else
    echo "File already ends with a newline."
fi

# Clean autom4te cache
echo "Cleaning autom4te cache..."
rm -rf autom4te.cache
echo "Cache cleaned."

# Verify the fix
echo "Verifying fix..."
echo "Last few lines of configure.ac:"
tail -3 configure.ac | cat -n
echo "--- End of file ---"

# Test if autoreconf works now
echo "Testing autoreconf..."
autoreconf --version > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "autoreconf is available, testing..."
    autoreconf --warnings=all --force --install 2>&1 | head -10
else
    echo "autoreconf not available in this environment"
fi