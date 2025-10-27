import os
with open('configure.ac', 'rb') as f: 
    content = f.read()
print(f"Original size: {len(content)}")
print(f"Ends with newline: {content.endswith(b'\\n')}")
if not content.endswith(b'\n'):
    with open('configure.ac', 'wb') as f: 
        f.write(content + b'\n')
    print("Added newline")
    with open('configure.ac', 'rb') as f: 
        new_content = f.read()
    print(f"New size: {len(new_content)}")
    print(f"Now ends with newline: {new_content.endswith(b'\\n')}")