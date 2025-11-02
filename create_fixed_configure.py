#!/usr/bin/env python3

# Read the current configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\n')}")

# Check if we need to add newline
if not content.endswith(b'\n'):
    print("File does not end with newline - fixing...")
    
    # Create backup
    with open('configure.ac.backup_before_newline_fix', 'wb') as f:
        f.write(content)
    
    # Add newline and write back
    fixed_content = content + b'\n'
    with open('configure.ac', 'wb') as f:
        f.write(fixed_content)
    
    print(f"Fixed size: {len(fixed_content)} bytes")
    print("✓ Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        verify = f.read()
    print(f"Verification - ends with newline: {verify.endswith(b'\n')}")
else:
    print("File already ends with newline")