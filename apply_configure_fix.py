#!/usr/bin/env python3

import os
import shutil

print("=== Fixing configure.ac newline issue ===")

# Step 1: Read current file and analyze
with open('/workspace/configure.ac', 'rb') as f:
    original_content = f.read()

print(f"Original file size: {len(original_content)} bytes")
print(f"Last 20 bytes: {repr(original_content[-20:])}")
print(f"Ends with newline: {original_content.endswith(b'\\n')}")

if original_content:
    last_char = original_content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Step 2: Apply fix if needed
if not original_content.endswith(b'\\n'):
    print("\\n=== Applying fix ===")
    
    # Create backup
    backup_path = '/workspace/configure.ac.backup_final'
    shutil.copy2('/workspace/configure.ac', backup_path)
    print(f"Created backup: {backup_path}")
    
    # Add newline
    fixed_content = original_content + b'\\n'
    
    # Write back
    with open('/workspace/configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print(f"Added newline. New size: {len(fixed_content)} bytes")
    
    # Step 3: Verify fix
    with open('/workspace/configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print("\\n=== Verification ===")
    print(f"New file size: {len(verify_content)} bytes")
    print(f"Size increase: {len(verify_content) - len(original_content)} bytes")
    print(f"Ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"Last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n') and len(verify_content) == len(original_content) + 1:
        print("\\n✅ SUCCESS: configure.ac now ends with newline!")
        
        # Step 4: Clean cache
        cache_path = '/workspace/autom4te.cache'
        if os.path.exists(cache_path):
            shutil.rmtree(cache_path)
            print("🧹 Cleaned autom4te.cache directory")
        
        print("\\n🎉 Fix complete! The m4 warning should be resolved.")
    else:
        print("\\n❌ ERROR: Fix verification failed!")
        # Restore backup
        shutil.copy2(backup_path, '/workspace/configure.ac')
        print("Restored from backup")
        
else:
    print("\\n✅ File already ends with newline - no fix needed")

print("\\nDone.")