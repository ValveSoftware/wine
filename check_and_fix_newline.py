#!/usr/bin/env python3

import os

# Check the current state of configure.ac
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Apply fix if needed
if not content.endswith(b'\\n'):
    print("\\nApplying fix: adding newline to configure.ac")
    
    # Write the content with newline
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(content + b'\\n')
    
    # Verify
    with open('/workspace/configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"After fix - file size: {len(new_content)} bytes")
    print(f"After fix - ends with newline: {new_content.endswith(b'\\n')}")
    
    if new_content.endswith(b'\\n'):
        print("SUCCESS: configure.ac now ends with newline!")
    else:
        print("ERROR: Fix failed")
else:
    print("File already ends with newline")

# Clean cache
if os.path.exists('/workspace/autom4te.cache'):
    import shutil
    shutil.rmtree('/workspace/autom4te.cache')
    print("Cleaned autom4te.cache")