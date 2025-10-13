#!/bin/bash

echo "=== Checking for syntax issues in configure.ac ==="

echo "1. Looking for empty commands (standalone colons):"
grep -n "^[[:space:]]*:[[:space:]]*$" /workspace/configure.ac || echo "None found"

echo -e "\n2. Looking for incomplete variable assignments:"
grep -n "^[[:space:]]*[A-Za-z_][A-Za-z0-9_]*=[[:space:]]*$" /workspace/configure.ac || echo "None found"

echo -e "\n3. Counting if/then/fi blocks:"
if_count=$(grep -c "^[[:space:]]*if[[:space:]]" /workspace/configure.ac)
fi_count=$(grep -c "^[[:space:]]*fi[[:space:]]*$" /workspace/configure.ac)
echo "if statements: $if_count"
echo "fi statements: $fi_count"

echo -e "\n4. Looking for unmatched brackets:"
open_brackets=$(grep -o '\[' /workspace/configure.ac | wc -l)
close_brackets=$(grep -o '\]' /workspace/configure.ac | wc -l)
echo "Open brackets [: $open_brackets"
echo "Close brackets ]: $close_brackets"

echo -e "\n5. Looking for unmatched parentheses:"
open_parens=$(grep -o '(' /workspace/configure.ac | wc -l)
close_parens=$(grep -o ')' /workspace/configure.ac | wc -l)
echo "Open parentheses (: $open_parens"
echo "Close parentheses ): $close_parens"

echo -e "\n6. Looking for incomplete macro calls:"
grep -n "WINE_[A-Z_]*(" /workspace/configure.ac | grep -v ").*$" || echo "None found"

echo -e "\n7. Looking for lines ending with backslash (continuation):"
grep -n "\\\\$" /workspace/configure.ac | tail -5

echo -e "\n8. Looking for the corrupted FLUIDSYNTH line:"
grep -n "tl100/tests)" /workspace/configure.ac || echo "Not found"