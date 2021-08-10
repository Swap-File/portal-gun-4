#ifndef _IO_H
#define _IO_H

#include "common/memory.h"

#define PIN_FAN_PWM		12 //physical pin
#define FAN_PWM_CHANNEL	0  //cpu channel
#define PIN_IR_PWM		13 //physical pin
#define IR_PWM_CHANNEL	1  //cpu channel

#define PIN_SERVO_SOFT_PWM   6

#define PIN_PRIMARY 4
#define PIN_ALT     17
#define PIN_MODE    22
#define PIN_RESET   23
#define PIN_REMOTE  24

#define BUTTON_PRIMARY_FIRE 0
#define BUTTON_ALT_FIRE 1
#define BUTTON_MODE_TOGGLE 2
#define BUTTON_RESET 3
#define BUTTON_LONG_RESET 4
#define BUTTON_NONE 5
#define BUTTON_LAST_LOCAL 5
#define BUTTON_REMOTE_PRIMARY_FIRE 6
#define BUTTON_REMOTE_ALT_FIRE 7
#define BUTTON_REMOTE_MODE_TOGGLE 8
#define BUTTON_REMOTE_RESET 9
#define BUTTON_REMOTE_LONG_RESET 10

#define SERVO_OPEN_TIME 2000
#define SERVO_CLOSED_TIME 1000

#define SERVO_NUM_PULSES 10

void io_init(void);
int io_update(struct gun_struct *this_gun);
int io_servo(bool closed);

#endif
