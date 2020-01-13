#ifndef _PORTAL_H
#define _PORTAL_H

#include <stdint.h>
#include "gstvideo/gstvideo.h"

#define GUN_EXPIRE 1000 //expire a gun in 1 second
#define LONG_PRESS_TIME  500
#define BUTTON_ACK_BLINK 100

#define BUTTON_PRIMARY_FIRE 0
#define BUTTON_ALT_FIRE 1
#define BUTTON_MODE_TOGGLE 2
#define BUTTON_RESET 3
#define BUTTON_LONG_RESET 4
#define BUTTON_NONE 5

#define MODE_DUO 2
#define MODE_SOLO 1

#define playlist_duo_SIZE 10
#define playlist_solo_SIZE 10
struct this_gun_struct {
	uint8_t mode = MODE_DUO;
	uint8_t brightness = 0;
	int8_t state_duo = 0;  //state reported to other gun
	int8_t state_duo_previous = 0;
	int8_t state_solo = 0; //internal state for single player modes
	int8_t state_solo_previous = 0;
	bool skip_states = false; //accelerate portal opening by skipping states
	uint32_t clock = 0;
	int  ir_pwm = 1024;
	int  fan_pwm = 1024;
	bool connected = false; 
	
	int8_t playlist_solo[playlist_solo_SIZE]={GST_LIBVISUAL_INFINITE,GST_LIBVISUAL_JESS,GST_GOOM,GST_GOOM2K1,GST_LIBVISUAL_JAKDAW ,GST_LIBVISUAL_OINKSIE,-1,-1,-1,-1};
	int8_t playlist_solo_index = 1;
	int8_t effect_solo = GST_VIDEOTESTSRC;

	int8_t playlist_duo[playlist_duo_SIZE]={GST_NORMAL,GST_EDGETV,GST_GLHEAT,GST_REVTV,GST_GLCUBE,GST_AGINGTV,-1,-1,-1,-1};
	int8_t playlist_duo_index = 1;
	int8_t effect_duo = GST_NORMAL;
	
	float latency = 0.0;
	float coretemp = 0.0;
	int  bw = 0;
	
	//imported from arduino
	int accel[3];
	int adc[4];
	
	float  battery_level_pretty=0;
	float  temperature_pretty=0;
};  


struct other_gun_struct {
	int state = 0; //state read from other gun
	int state_previous = 0;
	
	uint32_t last_seen = 0;
	uint32_t clock = 0;
};  

int piHiPri (const int pri);
unsigned int millis (void);



#endif