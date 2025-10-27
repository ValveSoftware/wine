#!/usr/bin/env python3

# Read configure.ac and show exact content
with open('configure.ac', 'r') as f:
    lines = f.readlines()

print(f"Total lines: {len(lines)}")
print("Last 5 lines:")
for i, line in enumerate(lines[-5:], start=len(lines)-4):
    print(f"Line {i}: {repr(line)}")

# Check if last line ends with newline
last_line = lines[-1] if lines else ""
print(f"\nLast line ends with \\n: {last_line.endswith('\\n')}")

# Show what we need to do
if not last_line.endswith('\n'):
    print("Need to add newline to last line")
    print(f"Current last line: {repr(last_line)}")
    print(f"Should be: {repr(last_line + '\\n')}")
    
    # Fix it
    lines[-1] = last_line + '\n'
    
    # Write back
    with open('configure.ac', 'w') as f:
        f.writelines(lines)
    
    print("Fixed!")
else:
    print("Already has newline")