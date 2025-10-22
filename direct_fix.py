import os
with open('configure.ac', 'rb') as f: content = f.read()
if not content.endswith(b'\n'): 
    with open('configure.ac', 'ab') as f: f.write(b'\n')
    print("Fixed: Added newline to configure.ac")
if os.path.exists('autom4te.cache'): 
    import shutil; shutil.rmtree('autom4te.cache')
    print("Cleaned: Removed autom4te.cache")