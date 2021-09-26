#include "pipe.h"
#include "io.h"
#include <sys/resource.h> //setpriority
#include <unistd.h> //sleep
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //system exit getenv
#include <string.h> //strstr
#include <sys/stat.h> //mkfifo  
#include "common/effects.h"

#define WEB_PRIMARY_FIRE	100
#define WEB_ALT_FIRE		101
#define WEB_MODE_TOGGLE		102
#define WEB_RESET			103

#define IFSTAT_BNEP 0
#define IFSTAT_WLAN 1

#define LASER_STATE_DELAYED_OFF	0
#define LASER_STATE_DELAYED_ON	1
#define LASER_STATE_OFF			2
#define LASER_STATE_ON			3

static int temp_in;
static int web_in;

static FILE *iw_fp;
static FILE *ifstat_wlan0_fp;
static FILE *ifstat_bnep0_fp;
static FILE *bash_fp;
static FILE *ping_fp;

void pipe_cleanup(void)
{
	printf("Killing old processes...\n");
	system("pkill gst*");
	system("pkill console");
	system("pkill projector");
	system("pkill mjpeg*");
}

void pipe_init(const struct gun_struct *this_gun)
{
	//let this priority get inherited to the children
	//setpriority(PRIO_PROCESS, getpid(), -10);

	system("/home/pi/portal/projector/projector &");
	while(this_gun->projector_loaded == false) {
		sleep(1);
	}

	system("sudo -E /home/pi/portal/console/console &");
	while(this_gun->console_loaded == false) {
		sleep(1);
	}

	//kick the core logic up to realtime for faster bit banging
	//piHiPri(40);

	bash_fp = popen("bash", "w");
	fcntl(fileno(bash_fp), F_SETFL, fcntl(fileno(bash_fp), F_GETFL, 0) | O_NONBLOCK);

	mkfifo ("/home/pi/FIFO_PIPE", 0777 );

	if ((web_in = open ("/home/pi/FIFO_PIPE",  ( O_RDONLY | O_NONBLOCK))) < 0) {
		perror("WEB_IN: Could not open named pipe for reading.");
		exit(-1);
	}

	fprintf(bash_fp, "sudo chown www-data /home/pi/FIFO_PIPE\n");
	fflush(bash_fp);

	if ((temp_in = open ("/sys/class/thermal/thermal_zone0/temp",  ( O_RDONLY | O_NONBLOCK))) < 0) {
		perror("Could not open /sys/class/thermal/thermal_zone0/temp for reading.");
		exit(-1);
	}

	if (this_gun->gordon)	ping_fp = popen("ping -i 0.2 192.168.3.21", "r");
	else					ping_fp = popen("ping -i 0.2 192.168.3.20", "r");

	fcntl(fileno(ping_fp), F_SETFL, fcntl(fileno(ping_fp), F_GETFL, 0) | O_NONBLOCK);

	ifstat_wlan0_fp = popen("ifstat -i wlan0", "r");
	fcntl(fileno(ifstat_wlan0_fp), F_SETFL, fcntl(fileno(ping_fp), F_GETFL, 0) | O_NONBLOCK);

	ifstat_bnep0_fp = popen("ifstat -i bnep0", "r");
	fcntl(fileno(ifstat_bnep0_fp), F_SETFL, fcntl(fileno(ping_fp), F_GETFL, 0) | O_NONBLOCK);

	iw_fp = popen("bash -c 'while true; do  iw dev wlan0 station dump; echo 'a'; sleep 1; done'", "r");
	fcntl(fileno(iw_fp), F_SETFL, fcntl(fileno(ping_fp), F_GETFL, 0) | O_NONBLOCK);

	//empty named pipe
	char buffer[100];
	while(read(web_in, buffer, sizeof(buffer)-1));
}

void pipe_audio(const char *filename)
{
	//use dmix for alsa so sounds can play over each other
	fprintf(bash_fp, "aplay -D dsp0 %s &\n",filename);
	fflush(bash_fp);
}

void pipe_laser_pwr(bool laser_request,struct gun_struct *this_gun)
{
	static uint8_t laser_state = LASER_STATE_OFF;

	if (this_gun == NULL) {
		if (laser_request == true) {
			fprintf(bash_fp, "vcgencmd display_power 1 2 &\n");
			laser_state = LASER_STATE_ON;
		}
		else if (laser_request == false) {
			fprintf(bash_fp, "vcgencmd display_power 0 2 &\n");
			laser_state = LASER_STATE_OFF;
		}
		fflush(bash_fp);
		return;
	}

	//this is fixed, laser_startup_delay is a function of the projector
	static uint32_t laser_startup_delay = 5000;

	//higher laser_shutdown_delay values will keep the laser on longer between shutting down for power savings
	//it may be 0 if we are operating without a shutter (immediate shutdown on request)
	uint32_t laser_shutdown_delay;
	if (this_gun->servo_bypass == SERVO_BYPASS)	laser_shutdown_delay = 0;
	else 										laser_shutdown_delay = 30000;

	static uint32_t startup_complete_time = 0;
	static uint32_t shutdown_complete_time = 0;

	//issue the startup command on request, and start the timer so we know when its done
	if (laser_state == LASER_STATE_OFF && laser_request == true) {
		printf("Starting Laser Warmup...\n");
		fprintf(bash_fp, "vcgencmd display_power 1 2 &\n");
		fflush(bash_fp);
		laser_state = LASER_STATE_DELAYED_ON;
		startup_complete_time = millis() + laser_startup_delay; //5 second laser warmup
	}

	//delayed startup countdown
	if (laser_state == LASER_STATE_DELAYED_ON) {
		uint32_t time_now = millis();
		if (startup_complete_time > time_now) {
			this_gun->laser_countdown = startup_complete_time - time_now;
		} else {
			this_gun->laser_countdown = 0;
			laser_state = LASER_STATE_ON;
		}
	}

	//if we get a shutdown request during startup, shut down immediately
	if (laser_state == LASER_STATE_DELAYED_ON && laser_request == false) {
		laser_state = LASER_STATE_OFF;
		this_gun->laser_countdown = 0;
		printf("Stopping Laser...\n");
		fprintf(bash_fp, "vcgencmd display_power 0 2 &\n");
		fflush(bash_fp);
	}

	//if we get a shutdown request during normal operation, delay the shutdown
	if (laser_state == LASER_STATE_ON && laser_request == false) {
		laser_state = LASER_STATE_DELAYED_OFF;
		shutdown_complete_time = millis() + laser_shutdown_delay;
	}

	//delayed shutdown countdown
	if (laser_state == LASER_STATE_DELAYED_OFF) {
		uint32_t time_now = millis();
		if (shutdown_complete_time < time_now) {
			printf("Stopping Laser...\n");
			fprintf(bash_fp, "vcgencmd display_power 0 2 &\n");
			fflush(bash_fp);
			laser_state = LASER_STATE_OFF;
		}
	}

	//if we get a turnon request during shutdown, go right back to being on
	if (laser_state == LASER_STATE_DELAYED_OFF && laser_request == true) {
		laser_state = LASER_STATE_ON;
	}

	if (laser_state == LASER_STATE_ON)			this_gun->laser_on = true;
	else if (laser_state == LASER_STATE_OFF)	this_gun->laser_on = false;

}

void pipe_www_out(const struct gun_struct *this_gun )
{
	static uint8_t web_packet_counter = 0;
	FILE *webout_fp;

	//send full playlist for editing
	webout_fp = fopen("/var/www/html/tmp/temp.txt", "w");
	fprintf(webout_fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %.2f %.2f %.2f %.2f %d %d %d\n",\
	this_gun->state_solo,this_gun->state_duo,this_gun->connected,this_gun->ir_pwm,\
	this_gun->playlist_solo[0],this_gun->playlist_solo[1],this_gun->playlist_solo[2],this_gun->playlist_solo[3],\
	this_gun->playlist_solo[4],this_gun->playlist_solo[5],this_gun->playlist_solo[6],this_gun->playlist_solo[7],\
	this_gun->playlist_solo[8],this_gun->playlist_solo[9],this_gun->effect_solo,\
	this_gun->playlist_duo[0],this_gun->playlist_duo[1],this_gun->playlist_duo[2],this_gun->playlist_duo[3],\
	this_gun->playlist_duo[4],this_gun->playlist_duo[5],this_gun->playlist_duo[6],this_gun->playlist_duo[7],\
	this_gun->playlist_duo[8],this_gun->playlist_duo[9],this_gun->effect_duo,\
	this_gun->battery_level_pretty,this_gun->temperature_pretty,this_gun->coretemp,\
	this_gun->latency,web_packet_counter,this_gun->mode,this_gun->servo_bypass);
	fclose(webout_fp);
	rename("/var/www/html/tmp/temp.txt","/var/www/html/tmp/portal.txt");

	web_packet_counter++;
}

int pipe_button_out(int button,bool gordon){
	
	static int button_last = BUTTON_NONE;
	if (button > BUTTON_NONE){
		if (button_last != button){
			button_last = button;
			printf("Remote Button %d\n",button);
			if(gordon){
				if (button == BUTTON_REMOTE_PRIMARY_FIRE)
					fprintf(bash_fp, "curl -d \"mode=1 100\" -X POST http://192.168.3.21/index_post.php &\n");
				if (button == BUTTON_REMOTE_ALT_FIRE)
					fprintf(bash_fp, "curl -d \"mode=1 101\" -X POST http://192.168.3.21/index_post.php &\n");
				if (button == BUTTON_REMOTE_MODE_TOGGLE)
					fprintf(bash_fp, "curl -d \"mode=1 102\" -X POST http://192.168.3.21/index_post.php &\n");	
				if (button == BUTTON_REMOTE_RESET)
					fprintf(bash_fp, "curl -d \"mode=1 103\" -X POST http://192.168.3.21/index_post.php &\n");		
				if (button == BUTTON_REMOTE_LONG_RESET)
					fprintf(bash_fp, "curl http://192.168.3.21/reboot.php &\n");
			}else{
				if (button == BUTTON_REMOTE_PRIMARY_FIRE)
					fprintf(bash_fp, "curl -d \"mode=1 100\" -X POST http://192.168.3.20/index_post.php &\n");
				if (button == BUTTON_REMOTE_ALT_FIRE)
					fprintf(bash_fp, "curl -d \"mode=1 101\" -X POST http://192.168.3.20/index_post.php &\n");
				if (button == BUTTON_REMOTE_MODE_TOGGLE)
					fprintf(bash_fp, "curl -d \"mode=1 102\" -X POST http://192.168.3.20/index_post.php &\n");
				if (button == BUTTON_REMOTE_RESET)
					fprintf(bash_fp, "curl -d \"mode=1 103\" -X POST http://192.168.3.20/index_post.php &\n");
				if (button == BUTTON_REMOTE_LONG_RESET)
					fprintf(bash_fp, "curl http://192.168.3.20/reboot.php &\n");
			}
			fflush(bash_fp);
		}
		return BUTTON_NONE;
	}
	return button;
}


int pipe_www_in(struct gun_struct *this_gun)
{
	int web_button = BUTTON_NONE;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0) { // dump entire buffer
		count = read(web_in, buffer, sizeof(buffer)-1);
		if (count > 1) { //ignore blank lines
			buffer[count-1] = '\0';
			//keep most recent line
			int tv[11];
			printf("MAIN Web Command: '%s'\n",buffer);
			int results = sscanf(buffer,"%d %d %d %d %d %d %d %d %d %d %d",\
			&tv[0],&tv[1],&tv[2],&tv[3],&tv[4],&tv[5],&tv[6],&tv[7],&tv[8],&tv[9],&tv[10]);
			if (results == 1) {
				if (tv[0] == 6) {
					this_gun->acces_counter++;
				}
			} else if (results == 2) {
				//button stuff
				if (tv[0] == 1) {
					switch (tv[1]) {
					case WEB_PRIMARY_FIRE:	web_button = BUTTON_PRIMARY_FIRE;  	break;
					case WEB_ALT_FIRE:		web_button = BUTTON_ALT_FIRE;    	break;
					case WEB_MODE_TOGGLE:	web_button = BUTTON_MODE_TOGGLE; 	break; 
					case WEB_RESET:     	web_button = BUTTON_RESET; 			break; 
					default: 				web_button = BUTTON_NONE;
					}
				}
				//ir stuff
				else if (tv[0] == 2 ) {
					if (tv[1] >= 0 && tv[1] <= 1024) this_gun->ir_pwm = tv[1];
				}
				//shutter stuff
				else if (tv[0] == 5 ) {
					this_gun->servo_bypass = tv[1];
				}
			} else if (results == 11) {
				//pad out playlist with repeat (-1)
				for (int i = 1; i < PLAYLIST_SIZE+1; i++) {
					if (tv[i] == GST_REPEAT) tv[i] = GST_REPEAT;
				}

				if (tv[0] == 3) { //set solo playlist
					for (int i = 0; i < PLAYLIST_SIZE; i++) {
						this_gun->playlist_solo[i] = tv[i+1];
					}
				} else if (tv[0] == 4) { //set duo playlist
					for (int i = 0; i < PLAYLIST_SIZE; i++) {
						this_gun->playlist_duo[i] = tv[i+1];
					}
				}
			}
		}
	}
	return web_button;
}

static void update_ping(float * ping) {
	static uint32_t ping_time = 0;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0) { // dump entire buffer
		count = read(fileno(ping_fp), buffer, sizeof(buffer)-1);
		if (count > 1) { //ignore blank lines
			buffer[count-1] = '\0';
			int placemarker = 0;
			int equals_pos = 0;
			while (placemarker < count-2) {
				if (buffer[placemarker] == '=') equals_pos++;
				placemarker++;
				if (equals_pos == 3) {
					sscanf(&buffer[placemarker],"%f", ping);
					ping_time = millis();
					break;
				}
			}
		}
	}
	if (millis() - ping_time > 1000) *ping = 0.0;
}

static void update_ifstat(int * kbytes,uint8_t unit) {
	static uint8_t bad_count = 0;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0) { // dump entire buffer
		if(unit == IFSTAT_BNEP)	count = read(fileno(ifstat_bnep0_fp), buffer, sizeof(buffer)-1);
		if(unit == IFSTAT_WLAN)	count = read(fileno(ifstat_wlan0_fp), buffer, sizeof(buffer)-1);
		if (count > 1) { //ignore blank lines
			buffer[count-1] = '\0';
			float in = 0;
			float out = 0;
			int result = sscanf(buffer,"%f %f", &in,&out);
			if(strstr(buffer, "n/a") != NULL) {
				*kbytes = -1;
			} else if (result == 2) {
				*kbytes = in + out;
				bad_count = 0;
			}
			else {
				if(bad_count < 255) bad_count++;
			}
		}
	}
	if (bad_count > 3)
	*kbytes = 0;
}

static void update_temp(float * temp) {
	static bool temp_first_cycle = true;
	static uint8_t temp_index = 0;
	static uint32_t temp_array[256];
	static uint32_t temp_total;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0) { // dump entire buffer
		count = read(temp_in, buffer, sizeof(buffer)-1);
		if (count > 0) { //ignore blank lines
			buffer[count-1] = '\0';
			int number;
			int result = sscanf(buffer,"%d", &number);
			if (number > 0 && number  < 85000 && result == 1) {
				number = (number * 9/5) + 32000;
				if (temp_first_cycle) {
					//preload filters with data if empty
					for (int i = 0; i < 256; i++) temp_array[i] = number;
					temp_total = 256*number;
					temp_first_cycle = false;
				}
				temp_total -= temp_array[temp_index];
				temp_array[temp_index] = number;
				temp_total += temp_array[temp_index++];
				float temp_temp = ((float)(temp_total >> 8)) / (1000.0);
				*temp = temp_temp;
			}
		}
	}
	lseek(temp_in,0,SEEK_SET);
}

static void update_iw(int * dbm, int * tx_bitrate) {
	static uint8_t bad_count = 0;
	int count = 1;
	char buffer[1000];
	count = read(fileno(iw_fp), buffer, sizeof(buffer)-1);
	if (count > 2) { // full message is 370 char
		char * signal_pointer = strstr(buffer,"signal:") + 7;
		char * tx_pointer = strstr(buffer,"tx bitrate:") + 11;
		char * end_pointer1 = strstr(signal_pointer,"\n");
		char * end_pointer2 = strstr(tx_pointer,"\n");
		if (signal_pointer != NULL && signal_pointer <  buffer + 1000 && \
				tx_pointer != NULL && tx_pointer < buffer + 1000 && \
				end_pointer1 != NULL && end_pointer1 < buffer + 1000 && \
				end_pointer2 != NULL && end_pointer2 < buffer + 1000 ) {
			*end_pointer1 = '\0';
			*end_pointer2 = '\0';
			int temp_dbm=0;
			int temp_tx_bitrate=0;
			int result1 = sscanf(signal_pointer,"%d", &temp_dbm);
			int result2 = sscanf(tx_pointer,"%d", &temp_tx_bitrate);
			if (result1 == 1 && result2 == 1) {
				*dbm = temp_dbm;
				*tx_bitrate = temp_tx_bitrate;
				bad_count = 0;
			} else {
				if(bad_count < 255) bad_count++;
			}
		}
	} else if (count == 2) { //2 means no signal, only getting the letter A
		bad_count++;
	}
	if (bad_count > 3) {
		*dbm = 0;
		*tx_bitrate = 0;
	}
}

void pipe_update(struct gun_struct *this_gun) {
	update_temp(&(this_gun->coretemp));
	update_ping(&(this_gun->latency));
	update_ifstat(&(this_gun->kbytes_wlan),IFSTAT_WLAN);
	update_ifstat(&(this_gun->kbytes_bnep),IFSTAT_BNEP);
	update_iw(&(this_gun->dbm), &(this_gun->tx_bitrate));
}
