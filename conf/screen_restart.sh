sudo pkill portal
sleep 1
sudo screen -S portal -p 0 -X stuff "/home/pi/portal/portal\n"
