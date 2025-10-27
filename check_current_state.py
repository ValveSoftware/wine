#!/usr/bin/env python3

# Check current state of configure.ac
with open('/workspace/configure.ac', 'rb') as f:
    content = f.read()
    
print(f"File size: {len(content)} bytes")
print(f"Last 30 bytes: {repr(content[-30:])}")
print(f"Ends with newline: {content.endswith(b'\\n')}")

if content:
    last_char = content[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")
    
# Show last few lines
with open('/workspace/configure.ac', 'r') as f:
    lines = f.read().split('\n')
    
print(f"Total lines when split by \\n: {len(lines)}")
print("Last 3 lines:")
for i, line in enumerate(lines[-3:], start=len(lines)-2):
    print(f"  Line {i}: {repr(line)}")