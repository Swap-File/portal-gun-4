<?php 

shell_exec('sudo systemctl stop hostapd');
sleep(1);

$channel = (int)$_POST["channel"];
if ($channel == 0)
	$channel = (int)$_GET["channel"];

switch ($channel) {
case 165:
	shell_exec('sudo cp /etc/hostapd/hostapd-165.conf /etc/hostapd/hostapd.conf');
	break;
case 36:
	shell_exec('sudo cp /etc/hostapd/hostapd-36.conf /etc/hostapd/hostapd.conf');
	break;
case 44:
	shell_exec('sudo cp /etc/hostapd/hostapd-44.conf /etc/hostapd/hostapd.conf');
	break;
case 149:
	shell_exec('sudo cp /etc/hostapd/hostapd-149.conf /etc/hostapd/hostapd.conf');
	break;
case 157:
	shell_exec('sudo cp /etc/hostapd/hostapd-157.conf /etc/hostapd/hostapd.conf');
	break;
}

shell_exec('sudo systemctl start hostapd');

?>

