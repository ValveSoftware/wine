#!/usr/bin/env python3

import os
import shutil
from datetime import datetime

print("=== CONFIGURE.AC NEWLINE FIX ===")

# Step 1: Create backup
backup_name = f'configure.ac.backup_{datetime.now().strftime("%Y%m%d_%H%M%S")}'
shutil.copy2('configure.ac', backup_name)
print(f"✓ Backup created: {backup_name}")

# Step 2: Read current file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original file size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Step 3: Apply fix if needed
if not content.endswith(b'\n'):
    print("✗ File does not end with newline - applying fix...")
    
    # Add newline
    fixed_content = content + b'\n'
    
    # Write fixed content
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print("✓ Newline added successfully!")
    
    # Verify the fix
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    
    print(f"New file size: {len(new_content)} bytes (+{len(new_content) - len(content)})")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
    
    if new_content.endswith(b'\\n') and len(new_content) == len(content) + 1:
        print("✓ Fix verified successfully!")
    else:
        print("✗ Fix verification failed!")
        # Restore backup
        shutil.copy2(backup_name, 'configure.ac')
        print(f"✓ Restored from backup: {backup_name}")
        exit(1)
        
else:
    print("✓ File already ends with newline - no fix needed")

# Step 4: Clean autom4te cache
if os.path.exists('autom4te.cache'):
    shutil.rmtree('autom4te.cache')
    print("✓ autom4te.cache removed")
else:
    print("✓ autom4te.cache not present")

print("\n=== FIX COMPLETED SUCCESSFULLY ===")
print("The configure.ac file now ends with a proper newline character.")
print("This should resolve the 'end of file treated as newline' error.")