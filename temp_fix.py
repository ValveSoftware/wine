#!/usr/bin/env python3

# Read the current configure.ac file
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Original size: {len(content)} bytes")
print(f"Ends with newline: {content.endswith(b'\\n')}")
print(f"Last 10 bytes: {repr(content[-10:])}")

# Add newline if needed
if not content.endswith(b'\n'):
    content += b'\n'
    with open('configure.ac', 'wb') as f:
        f.write(content)
    print("Newline added!")
    
    # Verify
    with open('configure.ac', 'rb') as f:
        new_content = f.read()
    print(f"New size: {len(new_content)} bytes")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")
else:
    print("Already has newline")