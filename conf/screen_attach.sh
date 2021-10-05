if [ ! "$(pidof screen)" ]; then
	echo "Starting screen, Restarting Portal..." 
    /home/pi/screen_restart.sh
fi

/home/pi/screen_resize.sh &
sudo screen -x -c "/home/pi/.screenrc"
