#ifndef _IO_H 
#define _IO_H

#include "common/memory.h"

#define PIN_FAN_PWM		12 //physical pin
#define FAN_PWM_CHANNEL	0  //cpu channel
#define PIN_IR_PWM		13 //physical pin
#define IR_PWM_CHANNEL	1  //cpu channel

#define PIN_SERVO_SOFT_PWM   25

#define PIN_PRIMARY 24
#define PIN_ALT     23
#define PIN_MODE    22 
#define PIN_RESET   27

#define BUTTON_PRIMARY_FIRE 0
#define BUTTON_ALT_FIRE 1
#define BUTTON_MODE_TOGGLE 2
#define BUTTON_RESET 3
#define BUTTON_LONG_RESET 4
#define BUTTON_NONE 5

#define SERVO_OPEN_TIME 1000
#define SERVO_CLOSED_TIME 2000

#define SERVO_NUM_PULSES 10

void io_init(void);
int io_update(const struct gun_struct *this_gun);
int io_servo(bool closed);

#endif