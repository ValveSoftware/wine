#!/usr/bin/env python3

import os
import shutil
from datetime import datetime

def fix_configure_newline():
    configure_path = '/workspace/configure.ac'
    
    # Create backup
    backup_path = f'{configure_path}.backup_{datetime.now().strftime("%Y%m%d_%H%M%S")}'
    shutil.copy2(configure_path, backup_path)
    print(f"Created backup: {backup_path}")
    
    # Read file in binary mode to preserve exact content
    with open(configure_path, 'rb') as f:
        content = f.read()
    
    print(f"Original file size: {len(content)} bytes")
    print(f"File ends with newline: {content.endswith(b'\\n')}")
    
    # If file doesn't end with newline, add one
    if not content.endswith(b'\n'):
        print("Adding newline character...")
        content += b'\n'
        
        # Write the corrected content back
        with open(configure_path, 'wb') as f:
            f.write(content)
        
        print("Newline added successfully!")
        print(f"New file size: {len(content)} bytes")
    else:
        print("File already ends with newline - no changes needed")
    
    # Clean autom4te cache
    cache_path = '/workspace/autom4te.cache'
    if os.path.exists(cache_path):
        shutil.rmtree(cache_path)
        print("Removed autom4te.cache directory")
    
    # Verify the fix
    with open(configure_path, 'rb') as f:
        final_content = f.read()
    
    print("\\n=== Verification ===")
    print(f"Final file size: {len(final_content)} bytes")
    print(f"Now ends with newline: {final_content.endswith(b'\\n')}")
    
    # Show last few lines
    with open(configure_path, 'r') as f:
        lines = f.readlines()
    
    print("\\nLast 3 lines:")
    for i, line in enumerate(lines[-3:], len(lines)-2):
        print(f"{i:4d}: {line.rstrip()}")
    
    print("\\n=== Fix completed successfully ===")

if __name__ == '__main__':
    fix_configure_newline()