#!/usr/bin/env python3
import os

# Check if we can read and write to configure.ac
try:
    with open('configure.ac', 'r') as f:
        content = f.read()
    print(f"Successfully read configure.ac, {len(content)} characters")
    
    # Check the ending
    if content.endswith('\n'):
        print("File ends with newline")
    else:
        print("File does NOT end with newline")
        print(f"Last 10 characters: {repr(content[-10:])}")
        
        # Try to fix it
        with open('configure.ac', 'w') as f:
            f.write(content + '\n')
        print("Added newline to file")
        
        # Verify
        with open('configure.ac', 'r') as f:
            new_content = f.read()
        if new_content.endswith('\n'):
            print("SUCCESS: File now ends with newline")
        else:
            print("ERROR: Still no newline")
            
except Exception as e:
    print(f"Error: {e}")