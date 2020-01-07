#include "portal.h"
#include "ledcontrol.h" 
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <bcm2835.h>

#define EFFECT_LENGTH 20  //length of the effect, doubled for output
#define LED_STRIP_LENGTH 20  //actual physical length (two stacks of 20 in parallel)

#define BREATHING_PERIOD 2 
#define EFFECT_RESOLUTION 400
#define BREATHING_RATE 2000

CRGB main_buffer_step1[EFFECT_LENGTH];
CRGB main_buffer_step2[EFFECT_LENGTH];
int timearray[EFFECT_LENGTH];

CRGB color1;
CRGB color2;
CRGB color1_previous;

uint8_t overlay = 0;

bool overlay_primer = true;
bool overlay_enabled = false;
int overlay_timer;

int timeoffset=0;
int offset_target_time = 0 ;

int led_index = 0;
int led_width_actual = 0;
int led_width_requested = 0	;
int color_update_index = 0;
int width_update_speed;
int total_offset_previous = 0;

int cooldown_time = 0; 

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

float effect_array[EFFECT_RESOLUTION];
int ticks_since_overlay_enable = 0; //disabled overlay on bootup
int width_update_speed_last_update = 0;
uint8_t brightnesslookup[256][EFFECT_RESOLUTION];

float ledcontrol_update(int width_temp,int width_speed_temp,int overlay_temp, int total_offset);

uint8_t led_update(const this_gun_struct& this_gun,const other_gun_struct& other_gun){
	
	//set color from state data		
	if (this_gun.state_duo > 0 || this_gun.state_solo > 0)	{
		color1 = CRGB(214,40,0);
		color2 = CRGB(0,30,224);
	}
	else if(this_gun.state_duo < 0 || this_gun.state_solo < 0)	{
		color1 = CRGB(0,30,224);
		color2 = CRGB(214,40,0);
	}
	else{
		color1 = CRGB(0,0,0);
		color2 = CRGB(0,0,0);
	}
	
	//set width
	int width_request = 20; //default is full fill
	
	if(this_gun.state_duo == 1 || this_gun.state_solo == -1 || this_gun.state_solo == 1 || this_gun.state_duo == -3 ){
		width_request = 10;
	}
	else if(this_gun.state_duo == -1)	width_request = 1;	
	else if(this_gun.state_duo == -2)	width_request = 5;	
	
	//set width update speed
	int width_update_speed = 200; //update every .2 seconds
	
	if (this_gun.state_duo <= -4 || this_gun.state_duo >= 4 || this_gun.state_solo <= -4 || this_gun.state_solo>= 4 ){
		width_update_speed = 0; //immediate change
	}
	
	int shutdown_effect = 0;
	//shutdown_effect
	if (this_gun.state_duo == 0 && this_gun.state_solo == 0) shutdown_effect = 1;
	
	uint32_t total_time_offset;
	if (this_gun.connected) {
		total_time_offset = (this_gun.clock >> 1) + (other_gun.clock >> 1);  //average the two values
	}else{
		total_time_offset = this_gun.clock;
	}
	total_time_offset = int((float)(total_time_offset % BREATHING_RATE) * ((float)EFFECT_RESOLUTION)/((float)BREATHING_RATE));
	
	return 255 * ledcontrol_update(width_request,width_update_speed,shutdown_effect,total_time_offset);
}

void ledcontrol_setup(void) {
	
	bcm2835_spi_begin();
	
	overlay_timer =  millis();
	
	printf("LED_Control: Building Lookup Table...\n");
	for ( int i = 0; i < EFFECT_RESOLUTION; i++ ) { 
		//add pi/2 to put max value (1) at start of range
		effect_array[i] =  ((exp(sin( M_PI/2  +(float(i)/(EFFECT_RESOLUTION/BREATHING_PERIOD)*M_PI))) )/ (M_E));
		//printf( "Y: %f\n", effect_array[i]);
	}
	
	for ( int x = 0; x < 256; x++ ) { 
		//printf( "X: %d Y:", x);
		for ( int y = 0; y < EFFECT_RESOLUTION; y++ ) { 
			brightnesslookup[x][y] = int(float(x * effect_array[y]));
			//printf( " %d", brightnesslookup[x][y]);
		}
		//printf( "\n");
	}
	
	width_update_speed_last_update = millis();
}

//colortemp not used!
float ledcontrol_update(int width_temp,int width_update_speed_temp,int overlay_temp, int  total_offset ) {

	//int start_time = micros();

	if (total_offset_previous > 200 && total_offset < 200 && led_width_actual == 20){
		//printf("RESET!\n");
		led_index = 0;
	}
	
	total_offset_previous = (total_offset_previous + 4) % 400;  //expected approximate change per cycle
	
	if (abs(total_offset - total_offset_previous) > 8){ //this gives it a bit more leeway  50 FPS, 2 seconds per offset, offset cycle of 400. 400/100 = 4
		//find shortest route 
		if((( total_offset - total_offset_previous + 400) % 400) < 200){  // add to total_offset_previous to reach total_offset
			total_offset_previous = ((total_offset_previous + 8)  % 400 ) ;
			//printf("this: %d previous %d adding to catchup!\n", total_offset, total_offset_previous);
		}else {// subtract from total_offset_previous to reach total_offset
			total_offset_previous = (total_offset_previous - 8 + 400) % 400;
			//printf("this: %d previous %d subbing to catchup!\n", total_offset, total_offset_previous);
		}		   
		total_offset = total_offset_previous; // use old value
	}
	//at 50 FPS, and 2 second effect time, and 20 LEDs, every 2 seconds we do 3 rotations
	//if we are at full width, reset the index to line it up

	total_offset_previous = total_offset;
	
	int time_this_cycle = millis();
	
	if (overlay_temp == 0){
		overlay_primer = true;
		overlay_enabled=false;
		overlay = 0x00;
	}else{
		if( overlay_primer == true && overlay_enabled== false){
			overlay = 0xFF;
			overlay_enabled= true;
			overlay_primer = false;
			overlay_timer = time_this_cycle;
		}
	}
	
	if (overlay_primer == true){
		
		if ((width_temp <= 20) && (width_temp >= 0)){
			led_width_requested = width_temp;
		}
		
		if (width_update_speed_temp >=0){
			width_update_speed = width_update_speed_temp;
		}
	}

	//on a color change, or coming back from zero width, go full bright on fill complete
	if( (color1_previous.r != color1.r || color1_previous.g != color1.g || color1_previous.b != color1.b) || ((led_width_requested !=0 && led_width_actual == 0 ) && overlay_enabled == false)){
		//adjust time offset, aiming to hit max brightness as color fill completes
		//offset to half of resolution for 50 FPS
		timeoffset = 300 - total_offset;
		cooldown_time = time_this_cycle;
		color_update_index = 0;
		color1_previous = color1;
	}
	
	//tweak breathing rate to attempt to keep in sync across network
	//only tweaks by 1 each cycle to be unnoticeable
	//only tweak when overlay not enabled

	if (time_this_cycle - cooldown_time > 1000){ 
		for ( int i = 0; i < EFFECT_LENGTH; i++ ){
			if(timearray[i] < 0){  // add to total_offset_previous to reach total_offset
				timearray[i] = timearray[i] + 1;
			}else if(timearray[i] > 0) {// subtract from total_offset_previous to reach total_offset
				timearray[i] = timearray[i] - 1;
			}	
		}
		
		if(timeoffset < 0){  // add to total_offset_previous to reach total_offset
			timeoffset = timeoffset + 1;
			//printf("Subbing to timearray! %d \n",timeoffset);
		}else if(timeoffset > 0){// subtract from total_offset_previous to reach total_offset
			timeoffset = timeoffset - 1;
			//printf("Adding to timearray! %d \n",timeoffset);
		}	

	}

	if (overlay_enabled == true){
		
		ticks_since_overlay_enable = time_this_cycle - overlay_timer;
		
		//printf("curtime %d\n", ticks_since_overlay_enable);
		
		if (ticks_since_overlay_enable > 127){  //only ramp to half brightness to save strip
			
			//overlay is now done, disable overlay
			//blank the buffer, and set all variables to 0
			overlay_enabled = false;
			led_width_requested = 0;
			led_width_actual = 0 ;
			color_update_index = 20;
			color1 = CRGB(0,0,0);
			color1_previous = color1;
			for ( int i = 0; i < EFFECT_LENGTH; i++ ){
				main_buffer_step1[i] = CRGB(0,0,0);
				timearray[i] = timeoffset;
			}
			overlay = 0x00;
			
		}else{	
			//during the overlay ramp up linearly, gives white output
			overlay = ticks_since_overlay_enable;
		}
	}
	
	//Update the color1 and time of lit LEDs
	if (color_update_index < led_width_actual){
		timearray[color_update_index] = timeoffset;
		main_buffer_step1[color_update_index] = color1;
		color_update_index = color_update_index + 1;
	}
	
	//width stuff
	if (time_this_cycle - width_update_speed_last_update > width_update_speed){
		//Narrow the lit section by blanking the index LED and incrementing the index.
		//Don't change time data for color2 LEDs
		if (led_width_requested < led_width_actual && led_width_requested >= 0){
			//printf("smaller\n" );
			led_width_actual--;
			main_buffer_step1[led_width_actual] = color2;
		}
		//Widen the lit section by copying the index LED color1 and time data and decrementing the index.
		else if( led_width_requested > led_width_actual  && led_width_requested <= EFFECT_LENGTH){
			//printf("bigger\n" );
			if (led_width_actual == 0  ){
				//starting an empty array
				main_buffer_step1[0] = color1;
				timearray[0] = timeoffset;
			}else{
				main_buffer_step1[led_width_actual] = main_buffer_step1[led_width_actual-1];
				timearray[led_width_actual] = timearray[led_width_actual - 1];
			}
			led_width_actual++;
			
			//supress color update code
			color_update_index++;
		}
		width_update_speed_last_update = time_this_cycle;
	}
	
	//printf("curtime %d\n", ((total_offset + timearray[0]) % EFFECT_RESOLUTION));
	
	for ( int i = 0; i < EFFECT_LENGTH; i++ ) {
		int curtime = 0;
		//dont apply brightness correction to first LED
		if (i != led_width_actual-1){
			curtime = (total_offset + timearray[i] + EFFECT_RESOLUTION) % EFFECT_RESOLUTION;
		}
		int current_location = (i + led_index) % EFFECT_LENGTH;
		
		main_buffer_step2[current_location].r = brightnesslookup[main_buffer_step1[i].r][curtime] | overlay;
		main_buffer_step2[current_location].g = brightnesslookup[main_buffer_step1[i].g][curtime] | overlay;
		main_buffer_step2[current_location].b = brightnesslookup[main_buffer_step1[i].b][curtime] | overlay;
	}
	
	
	//Shift array one LED forward and update index
	led_index = (led_index + 1) % EFFECT_LENGTH;
	
	//32 zeros is start of data frame, 32 is end of frame
	char output_buffer[4 + LED_STRIP_LENGTH*4 + 4];
	int i = 0; //physical led position
	
	//start of data
	for (int j = 0; j < 4; j++) output_buffer[i++] = 0x00;
	
	//led ring output
	for (int j = 0; j < EFFECT_LENGTH; j++){
		output_buffer[i++] = 0xFF;
		output_buffer[i++] = main_buffer_step2[j].b;
		output_buffer[i++] = main_buffer_step2[j].g;
		output_buffer[i++] = main_buffer_step2[j].r;
	}
	
	//end of data
	for (int j = 0; j < 4; j++) output_buffer[i++] = 0x00;

	bcm2835_spi_writenb(output_buffer, sizeof(output_buffer));
	//printf("Benchmark Microseconds! %d \n",micros()- start_time);
	
	//return a number representing the current breathing of the array for other LEDs to use
	return effect_array[(total_offset + timeoffset) % EFFECT_RESOLUTION];
}

void ledcontrol_wipe(void){
	
	//32 zeros is start of data frame, 16 is end
	char output_buffer[4 + LED_STRIP_LENGTH*4 + 4];
	int i = 0; //physical led position
	
	//start of data
	for (int j = 0; j < 4; j++) output_buffer[i++] = 0x00;
	
	//led ring 1 output
	for (int j = 0; j < LED_STRIP_LENGTH; j++){
		output_buffer[i++] = 0xFF;
		output_buffer[i++] = 0x00;
		output_buffer[i++] = 0x00;
		output_buffer[i++] = 0x00;
	}
	
	//end of data
	for (int j = 0; j < 4; j++) output_buffer[i++] = 0x00;
	
	bcm2835_spi_writenb(output_buffer, sizeof(output_buffer));
	bcm2835_spi_end();
}