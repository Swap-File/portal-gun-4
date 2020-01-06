#!/usr/bin/python3 -u

import time
import os
import stat

screen_on = False
if stat.S_ISFIFO(os.stat("/home/pi/GSTVIDEO_IN_PIPE").st_mode):
    file = open("/home/pi/GSTVIDEO_IN_PIPE","w") 
    while True:
        filename = input('Enter Request: ')
        
        if (filename.startswith( '8' ) and screen_on == False):
            os.system("vcgencmd display_power 1 2");
            time.sleep(3.5)
            screen_on = True;
            
        file.write(filename)
        file.write(" 0 0 0\n") 
        file.flush();           
       
        if (filename.startswith( '-' ) and screen_on == True):
            file.write("0 0 0 0 0\n") 
            file.flush();     
            os.system("vcgencmd display_power 0 2");
            screen_on = False;

        
else:
    print("/home/pi/GSTVIDEO_IN_PIPE doesn't Exist!")
    

