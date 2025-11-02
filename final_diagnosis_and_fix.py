#!/usr/bin/env python3

import os
import shutil
from datetime import datetime

filename = 'configure.ac'

print("=== CONFIGURE.AC NEWLINE DIAGNOSTIC AND FIX ===")
print()

# Step 1: Check if file exists
if not os.path.exists(filename):
    print(f"ERROR: {filename} does not exist!")
    exit(1)

# Step 2: Read and analyze current state
with open(filename, 'rb') as f:
    content = f.read()

print(f"File: {filename}")
print(f"Size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1]
    print(f"Last character: {repr(chr(last_char))} (ASCII {last_char})")
    
    if content.endswith(b'\\n'):
        print("✓ File already ends with newline - no fix needed!")
        exit(0)
    else:
        print("✗ File does NOT end with newline - fix needed")
else:
    print("ERROR: File is empty!")
    exit(1)

print()

# Step 3: Create backup
backup_name = f"{filename}.backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
print(f"Creating backup: {backup_name}")
shutil.copy2(filename, backup_name)

# Step 4: Apply fix
print("Applying newline fix...")
with open(filename, 'ab') as f:  # Open in append binary mode
    f.write(b'\\n')

# Step 5: Verify fix
with open(filename, 'rb') as f:
    new_content = f.read()

print()
print("=== VERIFICATION ===")
print(f"New size: {len(new_content)} bytes (was {len(content)})")
print(f"Size difference: +{len(new_content) - len(content)} bytes")
print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
print(f"Last 30 bytes: {repr(new_content[-30:])}")

if new_content.endswith(b'\\n') and len(new_content) == len(content) + 1:
    print("✓ SUCCESS: Fix applied correctly!")
    print(f"✓ Backup saved as: {backup_name}")
else:
    print("✗ ERROR: Fix verification failed!")
    print("Restoring from backup...")
    shutil.copy2(backup_name, filename)
    exit(1)

print()
print("=== NEXT STEPS ===")
print("1. Run: ./autogen.sh")
print("2. Check for autoreconf success")
print("3. If successful, you can remove the backup file")