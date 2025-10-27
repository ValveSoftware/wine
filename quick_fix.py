#!/usr/bin/env python3

import subprocess
import os

# Check current state
print("Checking configure.ac...")
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if not content.endswith(b'\\n'):
    print("File missing newline - applying fix...")
    
    # Use echo to append newline
    try:
        result = subprocess.run(['echo', ''], stdout=open('configure.ac', 'ab'), check=True)
        print("Applied newline using echo")
    except Exception as e:
        print(f"Echo method failed: {e}")
        # Fallback to Python method
        with open('configure.ac', 'ab') as f:
            f.write(b'\\n')
        print("Applied newline using Python")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"After fix:")
    print(f"  File size: {len(new_content)} bytes")
    print(f"  Ends with newline: {new_content.endswith(b'\\n')}")
    
    if new_content.endswith(b'\\n'):
        print("SUCCESS: Newline fix applied!")
    else:
        print("ERROR: Fix failed")
else:
    print("File already has proper newline ending")

# Clean cache
if os.path.exists('autom4te.cache'):
    import shutil
    shutil.rmtree('autom4te.cache')
    print("Cleaned autom4te.cache")