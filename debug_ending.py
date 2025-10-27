#!/usr/bin/env python3

# Debug the ending of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"Total file size: {len(content)} bytes")
print(f"Last 50 bytes as repr: {repr(content[-50:])}")
print(f"Last 10 bytes as hex: {content[-10:].hex()}")
print(f"Ends with \\n: {content.endswith(b'\\n')}")
print(f"Ends with \\r\\n: {content.endswith(b'\\r\\n')}")

# Check what the actual last character is
if len(content) > 0:
    last_char = content[-1:] 
    print(f"Last character: {repr(last_char)} (hex: {last_char.hex()})")

# Show the last few lines as text
try:
    text_content = content.decode('utf-8')
    lines = text_content.split('\n')
    print(f"\\nLast 5 lines:")
    for i, line in enumerate(lines[-5:], start=len(lines)-4):
        print(f"{i}: {repr(line)}")
except Exception as e:
    print(f"Error decoding as UTF-8: {e}")

# Now fix it
if not content.endswith(b'\n'):
    print("\\nAdding newline...")
    with open('configure.ac', 'wb') as f:
        f.write(content + b'\n')
    print("Fixed!")
else:
    print("\\nAlready has newline")