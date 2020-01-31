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

static int temp_in;
static int web_in;

static FILE *iw_fp;
static FILE *ifstat_wlan0_fp;
static FILE *ifstat_bnep0_fp;
static FILE *bash_fp;
static FILE *ping_fp;

void pipe_cleanup(void)
{
	printf("KILLING OLD PROCESSES\n");
	system("pkill gst*");
	system("pkill console");
	system("pkill projector");
	system("pkill mjpeg*");
}

void pipe_init(bool gordon)
{	
	//let this priority get inherited to the children
	setpriority(PRIO_PROCESS, getpid(), -10);

	system("sudo -E /home/pi/portal/console/console &");
	sleep(5);
	system("sudo -E /home/pi/portal/console/projector &");
	sleep(5);
	system("LD_LIBRARY_PATH=/usr/local/lib mjpg_streamer -i 'input_file.so -f /var/www/html/tmp -n snapshot.jpg' -o 'output_http.so -w /usr/local/www' &");
	
	//kick the core logic up to realtime for faster bit banging
	piHiPri(40);
	
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
	
	if (gordon)	ping_fp = popen("ping -i 0.2 192.168.3.21", "r");
	else		ping_fp = popen("ping -i 0.2 192.168.3.20", "r");

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
	fprintf(bash_fp, "aplay -D plug:dmix %s &\n",filename);
	fflush(bash_fp);
}

uint32_t pipe_laser_pwr(bool pwr)  //returns a countdown. 0 means request is complete.
{
	static bool current_power_state = false;
	static uint32_t event_complete_time = 0;
	
	if(pwr == true && current_power_state == false){
		printf("                                            Starting Laser Warmup...\n");
		fprintf(bash_fp, "vcgencmd display_power 1 2 &\n");
		fflush(bash_fp);
		current_power_state = true;
		event_complete_time = millis() + LASER_WARMUP_MS; //5 second laser warmup
	}
	
	if(pwr == false && current_power_state == true ){ 
	printf("                                            Laser Powerdown...\n");
		fprintf(bash_fp, "vcgencmd display_power 0 2 &\n");
		fflush(bash_fp);
		current_power_state = false;
		return 0;
	}
	
	uint32_t current_time = millis();
	if (event_complete_time < current_time) return 0;
	return event_complete_time - current_time;
}


void pipe_www_out(const struct gun_struct *this_gun )
{
	static uint8_t web_packet_counter = 0;
	FILE *webout_fp;
	
	//send full playlist for editing
	webout_fp = fopen("/var/www/html/tmp/temp.txt", "w");
	fprintf(webout_fp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %.2f %.2f %.2f %.2f %d %d\n" ,\
	this_gun->state_solo,this_gun->state_duo,this_gun->connected,this_gun->ir_pwm,\
	this_gun->playlist_solo[0],this_gun->playlist_solo[1],this_gun->playlist_solo[2],this_gun->playlist_solo[3],\
	this_gun->playlist_solo[4],this_gun->playlist_solo[5],this_gun->playlist_solo[6],this_gun->playlist_solo[7],\
	this_gun->playlist_solo[8],this_gun->playlist_solo[9],this_gun->effect_solo,\
	this_gun->playlist_duo[0],this_gun->playlist_duo[1],this_gun->playlist_duo[2],this_gun->playlist_duo[3],\
	this_gun->playlist_duo[4],this_gun->playlist_duo[5],this_gun->playlist_duo[6],this_gun->playlist_duo[7],\
	this_gun->playlist_duo[8],this_gun->playlist_duo[9],this_gun->effect_duo,\
	this_gun->battery_level_pretty,this_gun->temperature_pretty,this_gun->coretemp,\
	this_gun->latency,web_packet_counter,this_gun->mode);
	fclose(webout_fp);
	rename("/var/www/html/tmp/temp.txt","/var/www/html/tmp/portal.txt");

	web_packet_counter++;
}

int pipe_www_in(struct gun_struct *this_gun)
{
	int web_button = BUTTON_NONE;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0){ // dump entire buffer
		count = read(web_in, buffer, sizeof(buffer)-1);
		if (count > 1){ //ignore blank lines
			buffer[count-1] = '\0';
			//keep most recent line
			int tv[11];
			printf("MAIN Web Command: '%s'\n",buffer);
			int results = sscanf(buffer,"%d %d %d %d %d %d %d %d %d %d %d",\
			&tv[0],&tv[1],&tv[2],&tv[3],&tv[4],&tv[5],&tv[6],&tv[7],&tv[8],&tv[9],&tv[10]);
			if (results == 2) {
				//button stuff
				if (tv[0] == 1) {
					switch (tv[1]){
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

static void update_ping(float * ping){
	static uint32_t ping_time = 0;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0){ // dump entire buffer
		count = read(fileno(ping_fp), buffer, sizeof(buffer)-1);
		if (count > 1){ //ignore blank lines
			buffer[count-1] = '\0';
			int placemarker = 0;
			int equals_pos = 0;
			while (placemarker < count-2){
				if (buffer[placemarker] == '=') equals_pos++;
				placemarker++;
				if (equals_pos == 3){
					sscanf(&buffer[placemarker],"%f", ping);
					ping_time = millis();
					break;
				}
			}
		}
	}
	if (millis() - ping_time > 1000) *ping = 0.0;
}

static void update_ifstat(int * kbytes,uint8_t unit){	
	static uint8_t bad_count = 0;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0){ // dump entire buffer
		if(unit == IFSTAT_BNEP)	count = read(fileno(ifstat_bnep0_fp), buffer, sizeof(buffer)-1);
		if(unit == IFSTAT_WLAN)	count = read(fileno(ifstat_wlan0_fp), buffer, sizeof(buffer)-1);
		if (count > 1){ //ignore blank lines
			buffer[count-1] = '\0';
			float in = 0;
			float out = 0;
			int result = sscanf(buffer,"%f %f", &in,&out);
			if(strstr(buffer, "n/a") != NULL) {
				*kbytes = -1;
			}else if (result == 2) {
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

static void update_temp(float * temp){	
	static bool temp_first_cycle = true;
	static uint8_t temp_index = 0;
	static uint32_t temp_array[256];
	static uint32_t temp_total;
	int count = 1;
	char buffer[100];
	//stdin is line buffered so we can cheat a little bit
	while (count > 0){ // dump entire buffer
		count = read(temp_in, buffer, sizeof(buffer)-1);
		if (count > 0){ //ignore blank lines
			buffer[count-1] = '\0';
			int number;
			int result = sscanf(buffer,"%d", &number);
			if (number > 0 && number  < 85000 && result == 1){
				number = (number * 9/5) + 32000;
				if (temp_first_cycle){
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

static void update_iw(int * dbm, int * tx_bitrate){
	static uint8_t bad_count = 0;
	int count = 1;
	char buffer[1000];
	count = read(fileno(iw_fp), buffer, sizeof(buffer)-1);
	if (count > 2){ // full message is 370 char
		char * signal_pointer = strstr(buffer,"signal:") + 7;
		char * tx_pointer = strstr(buffer,"tx bitrate:") + 11;
		char * end_pointer1 = strstr(signal_pointer,"\n");
		char * end_pointer2 = strstr(tx_pointer,"\n");
		if (signal_pointer != NULL && signal_pointer <  buffer + 1000 && \
				tx_pointer != NULL && tx_pointer < buffer + 1000 && \
				end_pointer1 != NULL && end_pointer1 < buffer + 1000 && \
				end_pointer2 != NULL && end_pointer2 < buffer + 1000 ){
			*end_pointer1 = '\0';
			*end_pointer2 = '\0';
			int temp_dbm=0;
			int temp_tx_bitrate=0;
			int result1 = sscanf(signal_pointer,"%d", &temp_dbm);
			int result2 = sscanf(tx_pointer,"%d", &temp_tx_bitrate);
			if (result1 == 1 && result2 == 1){
				*dbm = temp_dbm;
				*tx_bitrate = temp_tx_bitrate;
				bad_count = 0;
			}else{
				if(bad_count < 255) bad_count++;
			}
		}
	}else if (count == 2){ //2 means no signal, only getting the letter A
		bad_count++;
	}
	if (bad_count > 3) {
		*dbm = 0;
		*tx_bitrate = 0;
	}
}

void pipe_update(struct gun_struct *this_gun){
	update_temp(&(this_gun->coretemp));
	update_ping(&(this_gun->latency));
	update_ifstat(&(this_gun->kbytes_wlan),IFSTAT_WLAN);
	update_ifstat(&(this_gun->kbytes_bnep),IFSTAT_BNEP);
	update_iw(&(this_gun->dbm), &(this_gun->tx_bitrate));
}