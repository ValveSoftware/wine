#!/usr/bin/env python3

import os
import shutil
from datetime import datetime

print("=== Configure.ac Newline Fix ===")

# Step 1: Diagnostic check
print("\\n1. Checking current state...")
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Step 2: Apply fix if needed
if not content.endswith(b'\\n'):
    print("\\n2. File does NOT end with newline - applying fix...")
    
    # Create backup
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_name = f'/workspace/configure.ac.backup_{timestamp}'
    shutil.copy2('/workspace/configure.ac', backup_name)
    print(f"Created backup: {backup_name}")
    
    # Add newline
    fixed_content = content + b'\\n'
    
    # Write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print(f"Added newline. New file size: {len(fixed_content)} bytes")
    
    # Step 3: Verify the fix
    print("\\n3. Verifying fix...")
    with open('/workspace/configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"Verification - file size: {len(verify_content)} bytes")
    print(f"Verification - ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"Verification - last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n') and len(verify_content) == len(content) + 1:
        print("\\n✅ SUCCESS: configure.ac now ends with newline!")
        
        # Step 4: Clean autom4te cache
        cache_dir = '/workspace/autom4te.cache'
        if os.path.exists(cache_dir):
            shutil.rmtree(cache_dir)
            print("🧹 Cleaned autom4te.cache directory")
        
        print("\\n🎉 Fix complete! The m4 warning should now be resolved.")
        
    else:
        print("\\n❌ ERROR: Fix verification failed!")
        print(f"Restoring from backup: {backup_name}")
        shutil.copy2(backup_name, '/workspace/configure.ac')
        
else:
    print("\\n✅ File already ends with newline - no fix needed")

print("\\nDone.")