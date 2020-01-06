<?php 
shell_exec('sudo /usr/bin/vcgencmd display_power 0');
sleep(10);
shell_exec('sudo /usr/bin/vcgencmd display_power 1');
?>

