#!/bin/bash

# Simple approach: use printf to append a newline
printf '\n' >> /workspace/configure.ac

echo "Newline appended to configure.ac"

# Clean cache
rm -rf /workspace/autom4te.cache 2>/dev/null || true

echo "Cache cleaned"