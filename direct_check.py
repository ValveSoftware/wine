#!/usr/bin/env python3

# Direct check and fix
with open('configure.ac', 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")
print(f"Last 15 bytes: {repr(data[-15:])}")

# Check if ends with newline
ends_with_newline = data.endswith(b'\\n')
print(f"Ends with newline: {ends_with_newline}")

if not ends_with_newline:
    print("FIXING: Adding newline to configure.ac")
    with open('configure.ac', 'wb') as f:
        f.write(data + b'\\n')
    print("Fix applied!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_data = f.read()
    print(f"After fix - ends with newline: {new_data.endswith(b'\\n')}")
else:
    print("File already has proper newline ending")