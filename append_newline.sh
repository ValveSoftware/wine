#!/bin/bash

# Simple script to append a newline to configure.ac
printf '\n' >> configure.ac
echo "Newline appended to configure.ac"

# Verify the change
echo "Last 3 lines of configure.ac:"
tail -3 configure.ac
echo "--- End of file ---"