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