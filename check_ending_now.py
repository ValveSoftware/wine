#!/usr/bin/env python3

# Check the exact ending of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()
    
print(f"File size: {len(content)} bytes")
print(f"Last 20 bytes: {content[-20:]}")
print(f"Last 20 bytes as hex: {content[-20:].hex()}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

# Show the last few characters
last_chars = content[-10:]
print(f"Last 10 characters: {repr(last_chars)}")

# Check if we need to add newline
if not content.endswith(b'\n'):
    print("\\nFile does NOT end with newline - needs fix!")
    print("Adding newline...")
    
    # Create backup
    with open('configure.ac.backup_before_newline_fix', 'wb') as f:
        f.write(content)
    
    # Add newline
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    
    print("Newline added successfully!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New file size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("\\nFile already ends with newline - no fix needed!")