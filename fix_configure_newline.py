#!/usr/bin/env python3

import os
import shutil
from datetime import datetime

print("Fixing configure.ac newline issue...")

# Step 1: Read current file in binary mode
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Current file size: {len(content)} bytes")
print(f"Last 20 bytes: {repr(content[-20:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Step 2: Check if fix is needed
if not content.endswith(b'\\n'):
    print("\\nFile does NOT end with newline - applying fix...")
    
    # Step 3: Create backup
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_name = f'configure.ac.backup_{timestamp}'
    shutil.copy2('configure.ac', backup_name)
    print(f"Created backup: {backup_name}")
    
    # Step 4: Add newline and write back
    fixed_content = content + b'\\n'
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print(f"Added newline. New file size: {len(fixed_content)} bytes")
    
    # Step 5: Verify the fix
    with open('configure.ac', 'rb') as f:
        verify_content = f.read()
    
    print(f"\\nVerification:")
    print(f"  File size: {len(verify_content)} bytes")
    print(f"  Ends with newline: {verify_content.endswith(b'\\n')}")
    print(f"  Last 20 bytes: {repr(verify_content[-20:])}")
    
    if verify_content.endswith(b'\\n'):
        print("\\n✅ SUCCESS: configure.ac now ends with newline!")
        
        # Step 6: Clean autom4te cache
        if os.path.exists('autom4te.cache'):
            shutil.rmtree('autom4te.cache')
            print("🧹 Cleaned autom4te.cache directory")
        
        print("\\n🎉 Fix complete! The m4 warning should now be resolved.")
        print("You can run ./autogen.sh to test the build.")
        
    else:
        print("\\n❌ ERROR: Fix verification failed!")
        print(f"Restoring from backup: {backup_name}")
        shutil.copy2(backup_name, 'configure.ac')
        
else:
    print("\\n✅ File already ends with newline - no fix needed")

print("\\nDone.")