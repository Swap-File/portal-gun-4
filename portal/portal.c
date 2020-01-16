#include "portal.h"
#include "sharedmem.h"
#include "i2cread.h"
#include "ledcontrol.h"
#include "udpcontrol.h"
#include "pipecontrol.h"
#include "statemachine.h"
#include <stdio.h>
#include <stdlib.h> //exit
#include <signal.h> //SIGPIPE
#include <unistd.h> //getuid
#include <bcm2835.h>

void INThandler(int dummy) {
	printf("\nCleaning up...\n");
	ledcontrol_wipe();
	pipecontrol_cleanup();
	exit(1);
}

int main(void){
	
	if (getuid()) {
		printf("Run as root!\n");
		exit(1);
	}

	pipecontrol_cleanup();
	
	struct gun_struct *this_gun;
	shm_setup(&this_gun,true );
	
	//catch broken pipes to respawn threads if they crash
	signal(SIGPIPE, SIG_IGN);
	//catch ctrl+c when exiting
	signal(SIGINT, INThandler);
	
	//stats
	uint32_t time_start = 0;
	int missed = 0;
	uint32_t time_fps = 0;
	int fps = 0;
	uint32_t time_delay = 0;
	int changes = 0;
	
	//setup libaries
	web_output(this_gun);
	bcm2835_init();
	ledcontrol_setup();
	i2creader_setup();	
	udpcontrol_setup();
	pipecontrol_setup();
	
	bool freq_50hz = true; //toggles every other cycle, cuts 100hz to 50hz

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
		this_gun->clock = millis();  //stop time for duration of frame
		freq_50hz = !freq_50hz;
		this_gun->state_duo_previous = this_gun->state_duo;
		this_gun->state_solo_previous = this_gun->state_solo;
		this_gun->other_gun_state_previous = this_gun->other_gun_state;
		
		//program code starts here
		update_temp(&(this_gun->coretemp));
		update_ping(&(this_gun->latency));
		update_ifstat(&(this_gun->kbytes_wlan),IFSTAT_WLAN);
		update_ifstat(&(this_gun->kbytes_bnep),IFSTAT_BNEP);
		update_iw(&(this_gun->dbm), &(this_gun->tx_bitrate));
		
		//read states from buttons
		int button_event = BUTTON_NONE;	
		button_event = io_update(this_gun);

		//if no event, read from the web
		if (button_event == BUTTON_NONE) button_event = read_web_pipe(this_gun);
		//read other gun's data, only if no button events are happening this cycle

		//if still no event, read button from the other gun for processing
		while (button_event == BUTTON_NONE){
			int result = udp_receive_state(&(this_gun->other_gun_state),&(this_gun->other_gun_clock));
			if (result <= 0) break;  //read until buffer empty
			else this_gun->other_gun_last_seen = this_gun->clock;  //update time data was seen
			if (millis() - this_gun->clock > 5) break; //flood protect
		}

		if (this_gun->video_done){
			if (this_gun->state_solo == 4) this_gun->state_solo = 3;
			else if (this_gun->state_solo == -4) this_gun->state_solo = -3;
			this_gun->video_done = false;
		}
						
		//process state changes
		local_state_engine(button_event,this_gun);
		if (button_event != BUTTON_NONE) changes++;

		//switch off updating the leds or i2c every other cycle, each takes about 1ms
		if(freq_50hz){ 
			this_gun->brightness = led_update(this_gun);
		}
		else{
			i2creader_update(this_gun);
		}
		
		audio_effects(this_gun);
	
		//send data to other gun
		static uint32_t time_udp_send = 0;
		if (this_gun->clock - time_udp_send > 100){
			udp_send_state(this_gun->state_duo,this_gun->clock);
			time_udp_send = this_gun->clock;
			web_output(this_gun);
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