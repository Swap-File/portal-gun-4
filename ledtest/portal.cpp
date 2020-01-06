#include "portal.h"

#include "ledcontrol.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <bcm2835.h>
#include <sched.h>

volatile bool video_done = false;

int piHiPri (const int pri)
{
  struct sched_param sched ;

  memset (&sched, 0, sizeof(sched)) ;

  if (pri > sched_get_priority_max (SCHED_RR))
    sched.sched_priority = sched_get_priority_max (SCHED_RR) ;
  else
    sched.sched_priority = pri ;

  return sched_setscheduler (0, SCHED_RR, &sched) ;
}

unsigned int millis (void){
  struct  timespec ts ;
  clock_gettime (CLOCK_MONOTONIC_RAW, &ts) ;
  uint64_t now  = (uint64_t)ts.tv_sec * (uint64_t)1000 + (uint64_t)(ts.tv_nsec / 1000000L) ;
  return (uint32_t)now ;
}


void INThandler(int dummy) {
	printf("\nCleaning up...\n");
	ledcontrol_wipe();
	exit(1);
}

void gstvideo_handler(int dummy) {
	printf("PORTAL: Video EoS caught, closing the portal...\n");
	video_done = true;
}



int main(void){
	
	if (getuid()) {
		printf("Run as root!\n");
		exit(1);
	}

	struct this_gun_struct this_gun;
	struct other_gun_struct other_gun;

	//catch broken pipes to respawn threads if they crash
	signal(SIGPIPE, SIG_IGN);
	//catch ctrl+c when exiting
	signal(SIGINT, INThandler);
	//catch signal from gstvideo when videos are done
	signal(SIGUSR2, gstvideo_handler);
	
	//stats
	uint32_t time_start = 0;
	int missed = 0;
	uint32_t time_fps = 0;
	int fps = 0;
	uint32_t time_delay = 0;
	int changes = 0;
	
	//setup libaries
	bcm2835_init();
	ledcontrol_setup();

	
	bool freq_50hz = false;
	
	while(1){
		//cycle start code - delay code
		time_start += 10;
		uint32_t predicted_delay = time_start - millis(); //calc predicted delay
		if (predicted_delay > 10) predicted_delay = 0; //check for overflow
		if (predicted_delay != 0){
			delay(predicted_delay); 
			time_delay += predicted_delay;
		}else{
			time_start = millis(); //reset timer to now
			printf("MAIN Skipping Idle...\n");
			missed++;
		}
		this_gun.clock = millis();  //stop time for duration of frame
		freq_50hz = !freq_50hz; 



		//switch off updating the leds or i2c every other cycle, each takes about 1ms
		if(freq_50hz){ 
			this_gun.brightness = led_update(this_gun,other_gun);
		}
	
		
		//cycle end code - fps counter and stats
		fps++;
		if (time_fps < millis()){		
			printf("MAIN FPS:%d mis:%d idle:%d%% changes:%d \n",fps,missed,time_delay/10,changes);
			fps = 0;
			time_delay = 0;
			time_fps += 1000;
			//readjust counter if we missed a cycle
			if (time_fps < millis()) time_fps = millis() + 1000;
		}	
	}
	return 0;
}