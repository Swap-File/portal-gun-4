<?php 
shell_exec('sudo pkill portal');
shell_exec("sudo screen -X stuff 'make clean\n'");
shell_exec("sudo screen -X stuff 'cat /home/pi/arpeture.txt\n'");
sleep(4); #delay to let display sync
shell_exec("sudo screen -X stuff 'make\n'");
shell_exec("sudo screen -X stuff '/home/pi/portal/portal\n'");
?>
