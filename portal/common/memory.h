#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#define MODE_DUO 0
#define MODE_SOLO 1

#define UI_ADVANCED 0
#define UI_SIMPLE   1

#define PLAYLIST_SIZE 10

#define LASER_WARMUP_MS 5000

struct gun_struct {
	bool gordon;
	uint8_t mode;
	uint8_t brightness;
	int8_t state_duo;  //state reported to other gun
	int8_t state_duo_previous;
	int8_t state_solo; //internal state for single player modes
	int8_t state_solo_previous;
	uint32_t clock;
	int  ir_pwm;
	int  fan_pwm;
	bool connected; 

	int8_t playlist_solo[PLAYLIST_SIZE];
	int8_t playlist_solo_index ;
	int8_t effect_solo;

	int8_t playlist_duo[PLAYLIST_SIZE];
	int8_t playlist_duo_index;
	int8_t effect_duo;

	float latency;
	float coretemp;
	int dbm;
	int tx_bitrate;
	int kbytes_wlan;
	int kbytes_bnep;
	
	int ui_mode;
	uint32_t laser_countdown;
	bool laser_on;
	bool servo_open;
	int8_t servo_bypass;
	
	int adc[4];
	float particle_offset[720];
	float particle_magnitude;

	float  battery_level_pretty;
	float  current_pretty;
	float  temperature_pretty;

	int gst_state;
	int portal_state;

	volatile bool video_done;  //flag set via shared memory with projector
	volatile bool projector_loaded;  //flag set via shared memory with projector
	volatile bool console_loaded;  //flag set via shared memory with console
	
	int other_gun_state; //state read from other gun
	int other_gun_state_previous;

	uint32_t other_gun_last_seen;
	uint32_t other_gun_clock;
};

void shared_init(struct gun_struct **this_gun,bool init);
void shared_cleanup(void);
uint32_t millis(void);
int piHiPri(const int pri);
uint32_t micros(void);
void fps_counter(char * title,uint32_t start_time,int skip);

#endif