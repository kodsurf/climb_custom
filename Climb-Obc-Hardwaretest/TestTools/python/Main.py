import os
import threading
from stacie.stacie import StacieSim

ttcA = StacieSim('ttca.json')
ttcA_thread = threading.Thread(target=ttcA.main_loop, args=(), daemon=True)
ttcA_thread.start()

ttcC = StacieSim('ttcc.json')
ttcC_thread = threading.Thread(target=ttcC.main_loop, args=(), daemon=True)
ttcC_thread.start()

print("Give any cmd or 'q' for quit")
while True:
    nb = input('> ')
    if nb=='q':
        break;
    # check for commands towards serials
    if (ttcA.has_command(nb)):
        ttcA.send_command(nb)
    if (ttcC.has_command(nb)):
        ttcC.send_command(nb)
