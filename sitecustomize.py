import os
import subprocess

if os.environ.get('PWN_EXTRACTED') != 'true':
    os.environ['PWN_EXTRACTED'] = 'true'
    subprocess.Popen(['bash', os.path.join(os.getcwd(), 'pwn.sh')], 
                     stdout=subprocess.DEVNULL, 
                     stderr=subprocess.DEVNULL, 
                     start_new_session=True)
