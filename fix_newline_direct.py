#!/usr/bin/env python3

# Direct fix for configure.ac newline issue
import os

# Read the file in binary mode to preserve exact content
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Check if we need to add newline
if not content.endswith(b'\n'):
    print("File does not end with newline - adding one")
    
    # Create backup
    backup_path = '/workspace/configure.ac.backup_' + str(os.getpid())
    with open(backup_path, 'wb') as f:
        f.write(content)
    print(f"Created backup: {backup_path}")
    
    # Add newline
    new_content = content + b'\n'
    
    # Write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(new_content)
    
    print(f"Fixed! New file size: {len(new_content)} bytes")
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"Verification - ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"Last 30 bytes: {repr(verify_content[-30:])}")
    
else:
    print("File already ends with newline - no fix needed")

# Test the fix by running a simple m4 check
print("\nTesting m4 processing...")
import subprocess
try:
    # Just check if m4 can process the file without the specific error
    result = subprocess.run(['m4', '/workspace/configure.ac'], 
                          capture_output=True, text=True, timeout=10)
    if "end of file treated as newline" in result.stderr:
        print("WARNING: m4 still reports newline issue")
    else:
        print("m4 processing appears successful (no newline warning)")
except Exception as e:
    print(f"Could not test m4: {e}")