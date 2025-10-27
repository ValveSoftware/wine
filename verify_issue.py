#!/usr/bin/env python3

# Verify the exact issue with configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print("=== CONFIGURE.AC DIAGNOSTIC ===")
print(f"File size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline (\\n): {content.endswith(b'\\n')}")

# Check the last character
if len(content) > 0:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")
    
    if last_char == b':':
        print("✗ ISSUE CONFIRMED: File ends with ':' character, missing newline!")
        print("  This will cause: 'Warning: end of file treated as newline'")
    elif last_char == b'\n':
        print("✓ File correctly ends with newline")
    else:
        print(f"? File ends with unexpected character: {last_char}")

# Show the actual last line
try:
    text_content = content.decode('utf-8')
    lines = text_content.split('\n')
    print(f"\nLast few lines:")
    for i, line in enumerate(lines[-3:], start=len(lines)-2):
        if i > 0:
            print(f"Line {i}: {repr(line)}")
    
    print(f"\nTotal lines: {len(lines)}")
    print(f"Last line is empty (indicates trailing newline): {lines[-1] == ''}")
    
except Exception as e:
    print(f"Error decoding file: {e}")

print("\n=== END DIAGNOSTIC ===")