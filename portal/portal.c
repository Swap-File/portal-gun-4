#include "common/memory.h"
#include "i2c.h"
#include "io.h"
#include "led.h"
#include "udp.h"
#include "pipe.h"
#include "state.h"
#include <stdio.h>
#include <stdlib.h> //exit
#include <signal.h> //SIGPIPE
#include <unistd.h> //getuid
#include <bcm2835.h>

void INThandler(int dummy) {
	printf("\nCleaning up...\n");
	pipe_laser_pwr(true,NULL);
	led_wipe();
	pipe_cleanup();
	shared_cleanup();
	exit(1);
}

int main(void)
{
	if (getuid()) {
		printf("Run as root!\n");
		exit(1);
	}

	/* cleanup incase of straggling processes */
	pipe_cleanup();
	
	struct gun_struct *this_gun;
	shared_init(&this_gun,true);
	
	/* catch broken pipes */
	signal(SIGPIPE, SIG_IGN);
	/* catch ctrl+c for exit */
	signal(SIGINT, INThandler);
	
	/* setup libraries */
	pipe_www_out(this_gun); //output initial data now to let website load during gun init
	bcm2835_init();
	led_init();
	io_init();
	i2c_init();	
	udp_init(this_gun->gordon);

	pipe_init(this_gun);

	/* toggles every other cycle, cuts 100hz core tick speed to 50hz */
	bool freq_50hz = true;
	
	/* reset timer to now to avoid missing first cycle */
	uint32_t time_start = millis(); 

	while (1) {
		bool frameskip = false;
		/* Cycle Delay */
		time_start += 10;
		uint32_t predicted_delay = time_start - millis();
		if (predicted_delay > 10) predicted_delay = 0; //check for overflow
		if (predicted_delay != 0){
			/* Update servos at 20ms intervals assuming we have enough delay, otherwise skip it */
			if (predicted_delay >= 2 && freq_50hz){
			
				predicted_delay -= io_servo(this_gun->servo_on);
			}
			delay(predicted_delay); 
		} else {
			time_start = millis(); //reset timer to now to skip idle
			frameskip = true;
		}
		
		/* Cycle Setup */
		this_gun->clock = millis(); //stop time for duration of frame
		freq_50hz = !freq_50hz;
		
		this_gun->state_duo_previous = this_gun->state_duo;
		this_gun->state_solo_previous = this_gun->state_solo;
		this_gun->other_gun_state_previous = this_gun->other_gun_state;

		/* Program Code */
		pipe_update(this_gun);
	
		/* Read Physical Buttons */
		int button_event = io_update(this_gun);

		/* Otherwise Read Web Buttons */
		if (button_event == BUTTON_NONE) button_event = pipe_www_in(this_gun);
		//read other gun's data, only if no button events are happening this cycle

		/* Otherwise Read Other Gun */
		while (button_event == BUTTON_NONE) {
			int result = udp_receive_state(&(this_gun->other_gun_state),&(this_gun->other_gun_clock));
			if (result <= 0) break;  //read until buffer empty
			else this_gun->other_gun_last_seen = this_gun->clock;  //update time data was seen
			if (millis() - this_gun->clock > 5) break; //flood protect
		}
						
		/* Process State changes */
		state_engine(button_event,this_gun);

		/* Alternate blocking tasks to keep core at 100hz */
		if(freq_50hz)
			this_gun->brightness = led_update(this_gun);
		else
			i2c_update(this_gun);
		
		/* Send data to other gun and www output */
		static uint32_t time_udp_send = 0;
		if (this_gun->clock - time_udp_send > 100) {
			udp_send_state(this_gun->state_duo,this_gun->clock);
			time_udp_send = this_gun->clock;
			pipe_www_out(this_gun);
		}
		
		/* FPS counter */
		fps_counter("Portal:    ",this_gun->clock * 1000,frameskip);
	}
	return 0;
}