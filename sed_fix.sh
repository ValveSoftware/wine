#!/bin/bash
# Use sed to ensure the file ends with a newline
sed -i -e '$a\' configure.ac
echo "Newline added using sed"