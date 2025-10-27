#!/usr/bin/env python3

# Execute the newline fix
print("Executing newline fix for configure.ac...")

# Read current file
with open('configure.ac', 'rb') as f:
    data = f.read()

print(f"Current file size: {len(data)} bytes")
print(f"Ends with newline: {data.endswith(b'\\n')}")

if not data.endswith(b'\\n'):
    print("File does not end with newline - fixing...")
    
    # Append newline
    with open('configure.ac', 'ab') as f:
        f.write(b'\\n')
    
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_data = f.read()
    
    print(f"New file size: {len(new_data)} bytes")
    print(f"Now ends with newline: {new_data.endswith(b'\\n')}")
    
    if new_data.endswith(b'\\n'):
        print("\\n✓ SUCCESS: configure.ac now has proper newline ending!")
        print("\\nThe autoreconf issue should now be resolved.")
        print("You can run ./autogen.sh to test the build.")
    else:
        print("\\n✗ ERROR: Fix failed - file still doesn't end with newline")
        
else:
    print("File already ends with newline - no fix needed")

# Show last few bytes for verification
with open('configure.ac', 'rb') as f:
    final_data = f.read()

print(f"\\nFinal verification:")
print(f"Last 30 bytes: {repr(final_data[-30:])}")

# Clean autom4te cache if it exists
import os
import shutil

if os.path.exists('autom4te.cache'):
    shutil.rmtree('autom4te.cache')
    print("Cleaned autom4te.cache directory")

print("\\nFix complete!")