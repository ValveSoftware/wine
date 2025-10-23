#!/usr/bin/env python3

# Check the current state of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 10 bytes: {repr(content[-10:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if not content.endswith(b'\n'):
    print("\\nFile needs a newline at the end!")
    print("To fix this, append a single newline character to the file.")
    
    # Show what the fix would look like
    fixed_content = content + b'\n'
    print(f"After fix - size would be: {len(fixed_content)} bytes")
    print(f"After fix - would end with newline: {fixed_content.endswith(b'\\n')}")
else:
    print("\\nFile already ends with newline - no fix needed.")