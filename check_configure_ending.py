#!/usr/bin/env python3

# Check the exact ending of configure.ac
with open('configure.ac', 'rb') as f:
    content = f.read()

print(f"File size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")

# Also check in text mode
with open('configure.ac', 'r') as f:
    text_content = f.read()

print(f"Text mode - ends with newline: {text_content.endswith('\\n')}")
if text_content:
    print(f"Text mode - last character: {repr(text_content[-1])}")
    
# Show the last few lines
lines = text_content.split('\\n')
print(f"Total lines when split by \\n: {len(lines)}")
print("Last 3 lines:")
for i, line in enumerate(lines[-3:], start=len(lines)-2):
    print(f"  Line {i}: {repr(line)}")