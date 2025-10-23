#!/bin/bash

# Add a newline to configure.ac if it doesn't end with one
echo "" >> configure.ac
echo "Newline added to configure.ac"

# Remove autom4te cache
rm -rf autom4te.cache 2>/dev/null || true
echo "Cache cleaned"