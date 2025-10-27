#!/bin/bash

# Simple fix: append a newline to configure.ac
echo "" >> configure.ac

echo "Newline appended to configure.ac"

# Clean cache
rm -rf autom4te.cache 2>/dev/null || true

echo "Cache cleaned"
echo "Fix completed"