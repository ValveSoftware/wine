with open('configure.ac', 'rb') as f:
    content = f.read()
    print("Last 20 bytes:", content[-20:])
    print("Ends with newline:", content.endswith(b'\n'))
    print("Last character as hex:", hex(content[-1]) if content else "empty file")