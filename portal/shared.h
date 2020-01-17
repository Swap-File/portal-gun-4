#ifndef _SHARED_H
#define _SHARED_H

#include <stdint.h>
#include <stdbool.h>

#define MODE_DUO 2
#define MODE_SOLO 1

#define PLAYLIST_SIZE 10

struct gun_struct {
	bool gordon;
	uint8_t mode;
	uint8_t brightness;
	int8_t state_duo;  //state reported to other gun
	int8_t state_duo_previous;
	int8_t state_solo; //internal state for single player modes
	int8_t state_solo_previous;
	bool skip_states; //accelerate portal opening by skipping states
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

	int accel[3];
	int adc[4];

	float  battery_level_pretty;
	float  temperature_pretty;

	int gst_state;
	int ahrs_state;

	bool video_done;  //flag set via shared memory with gstvideo

	int other_gun_state; //state read from other gun
	int other_gun_state_previous;

	uint32_t other_gun_last_seen;
	uint32_t other_gun_clock;
};

void shm_init(struct gun_struct **this_gun,bool init);
void shm_cleanup(void);
uint32_t millis(void);
int piHiPri(const int pri);

#endif