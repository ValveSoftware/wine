#!/usr/bin/env python3

# Show the exact end of configure.ac to help with str_replace
with open('configure.ac', 'r') as f:
    content = f.read()

print("Last 200 characters:")
print(repr(content[-200:]))

print("\nLast 5 lines split by \\n:")
lines = content.split('\n')
for i, line in enumerate(lines[-5:], start=len(lines)-4):
    print(f"Line {i}: {repr(line)}")

print(f"\nTotal lines: {len(lines)}")
print(f"Last line: {repr(lines[-1])}")
print(f"Second to last line: {repr(lines[-2])}")

# Check if there's a trailing newline
if content.endswith('\n'):
    print("File ends with newline")
else:
    print("File does NOT end with newline")
    print(f"Last character: {repr(content[-1])}")

# Run the diagnostic
import subprocess
import sys

try:
    result = subprocess.run([sys.executable, __file__.replace('show_exact_end.py', 'show_exact_end.py')], 
                          capture_output=True, text=True, cwd='/workspace')
    print("Diagnostic output:")
    print(result.stdout)
    if result.stderr:
        print("Errors:")
        print(result.stderr)
except Exception as e:
    print(f"Error running diagnostic: {e}")
    
# Now let's check the file directly
print("\n=== DIRECT FILE CHECK ===")
with open('/workspace/configure.ac', 'rb') as f:
    content_bytes = f.read()
    
print(f"File size: {len(content_bytes)} bytes")
print(f"Last 20 bytes: {repr(content_bytes[-20:])}")
print(f"Ends with newline: {content_bytes.endswith(b'\\n')}")

if content_bytes:
    last_char = content_bytes[-1:]
    print(f"Last character: {repr(last_char)} (hex: 0x{last_char.hex()})")