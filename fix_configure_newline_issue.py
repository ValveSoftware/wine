#!/usr/bin/env python3

import os
import shutil

print("Checking configure.ac newline status...")

# Read the current file in binary mode
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# If it doesn't end with newline, fix it
if not content.endswith(b'\\n'):
    print("\\nFile does NOT end with newline - applying fix...")
    
    # Create backup
    backup_name = '/workspace/configure.ac.backup_newline_fix'
    shutil.copy2('/workspace/configure.ac', backup_name)
    print(f"Created backup: {backup_name}")
    
    # Add newline
    fixed_content = content + b'\\n'
    
    # Write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print(f"Added newline. New file size: {len(fixed_content)} bytes")
    
    # Verify the fix
    with open('/workspace/configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"\\nVerification:")
    print(f"  File size: {len(verify_content)} bytes")
    print(f"  Ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"  Last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n'):
        print("\\n✅ SUCCESS: configure.ac now ends with newline!")
        
        # Clean autom4te cache if it exists
        cache_dir = '/workspace/autom4te.cache'
        if os.path.exists(cache_dir):
            shutil.rmtree(cache_dir)
            print("🧹 Cleaned autom4te.cache directory")
        
        print("\\n🎉 Fix complete! The m4 warning should now be resolved.")
        
    else:
        print("\\n❌ ERROR: Fix verification failed!")
        
else:
    print("\\n✅ File already ends with newline - no fix needed")

print("\\nDone.")