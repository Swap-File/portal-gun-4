<?php
echo file_get_contents( "tmp/portal.txt" );
$file = fopen("/home/pi/FIFO_PIPE","a") or die("check owner of FIFO_PIPE and make sure it's already created!");
echo fwrite($file, "6\n");
fflush ($file);
fclose ($file);
?>