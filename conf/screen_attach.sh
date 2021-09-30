if [ ! "$(pidof screen)" ]; then
	echo "Starting screen, Restarting Portal..." 
    /home/pi/screen_restart.sh
	echo -n "8" 
	sleep 1
	echo -n "7" 
	sleep 1
	echo -n "6" 
	sleep 1
	echo -n "5" 
	sleep 1
	echo -n "4" 
	sleep 1
	echo -n "3" 
	sleep 1
	echo -n "2" 
	sleep 1
	echo -n  "1" 
	sleep 1
	echo -n  "0" 
fi

sudo screen -S portal -p 0 -X stuff "^aF; \n\n\nReminder:\nctrl a ctrl d to detach\nctrl a shift f to fit width\n\n\n\n"
sudo screen -x
