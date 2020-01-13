#ifndef _PIPECONTROL_H 
#define _PIPECONTROL_H

#define WEB_PRIMARY_FIRE 100
#define WEB_ALT_FIRE 101
#define WEB_MODE_TOGGLE 102
#define WEB_RESET 103

#define PIN_FAN_PWM  12
#define FAN_PWM_CHANNEL  0 

#define PIN_IR_PWM   13 
#define IR_PWM_CHANNEL 1

#define PIN_PRIMARY 24
#define PIN_ALT     23
#define PIN_MODE    22 
#define PIN_RESET   27

void pipecontrol_setup(void);
void pipecontrol_cleanup(void);
void aplay(const char *filename);
int io_update(const this_gun_struct& this_gun);
void web_output(const this_gun_struct& this_gun);
void gstvideo_command(const this_gun_struct& this_gun,int clock);
void audio_effects(const this_gun_struct& this_gun);
int read_web_pipe(this_gun_struct& this_gun);
void update_ping(float * ping);
void update_temp(float * temp);
void update_bw(int * bw);
#endif