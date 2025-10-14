#!/bin/bash
# Check if configure.ac ends with newline
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with newline"
    echo "Last character:"
    tail -c1 configure.ac | od -c
else
    echo "File ends with newline"
fi