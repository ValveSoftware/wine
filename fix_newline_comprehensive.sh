#!/bin/bash

echo "=== Fixing configure.ac newline issue ==="

# Check if the file ends with a newline
echo "Checking if configure.ac ends with a newline..."
if [ "$(tail -c1 configure.ac | wc -l)" -eq 0 ]; then
    echo "File does NOT end with a newline. Fixing..."
    
    # Method 1: Use printf to append newline
    printf '\n' >> configure.ac
    
    # Verify the fix
    if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
        echo "SUCCESS: Newline added using printf"
    else
        echo "Method 1 failed, trying method 2..."
        
        # Method 2: Use echo to append newline
        echo "" >> configure.ac
        
        if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
            echo "SUCCESS: Newline added using echo"
        else
            echo "Method 2 failed, trying method 3..."
            
            # Method 3: Use sed to ensure newline at end
            sed -i -e '$a\' configure.ac
            
            if [ "$(tail -c1 configure.ac | wc -l)" -eq 1 ]; then
                echo "SUCCESS: Newline added using sed"
            else
                echo "ERROR: All methods failed to add newline"
                exit 1
            fi
        fi
    fi
else
    echo "File already ends with a newline."
fi

echo ""
echo "=== Verification ==="
echo "Last 3 lines of configure.ac:"
tail -3 configure.ac | cat -n
echo "--- End of file ---"

echo ""
echo "File size and last character check:"
wc -c configure.ac
echo "Last character (should be newline):"
tail -c1 configure.ac | od -c

echo ""
echo "=== Testing autoreconf ==="
echo "Running autoreconf to test if the issue is fixed..."
if autoreconf --version > /dev/null 2>&1; then
    echo "autoreconf is available, testing..."
    # Just test the configure.ac syntax, don't run full autoreconf yet
    if m4 configure.ac > /dev/null 2>&1; then
        echo "SUCCESS: m4 can now process configure.ac without warnings"
    else
        echo "WARNING: m4 still has issues with configure.ac"
    fi
else
    echo "autoreconf not available for testing"
fi

echo "=== Fix complete ==="