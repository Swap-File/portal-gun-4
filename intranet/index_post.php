
<?php 
$file = fopen("/home/pi/FIFO_PIPE","a") or die("check owner of FIFO_PIPE and make sure it's already created!");
echo fwrite($file, $_POST["mode"] . "\n");
fflush ($file);
fclose ($file);
?>