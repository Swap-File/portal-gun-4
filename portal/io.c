#include "io.h"
#include <bcm2835.h>
#include <stdio.h>

#define DEBOUNCE_COUNT 3

#define RESET_HOLD_DELAY 2000
#define RESET_COUNTDOWN 10000

void io_init(void)
{
	bcm2835_gpio_fsel(PIN_SERVO_SOFT_PWM, BCM2835_GPIO_FSEL_OUTP);

	bcm2835_gpio_fsel(PIN_FAN_PWM, BCM2835_GPIO_FSEL_ALT0); //PWM_CHANNEL 0
	bcm2835_gpio_fsel(PIN_IR_PWM, BCM2835_GPIO_FSEL_ALT0);  //PWM_CHANNEL 1

	bcm2835_pwm_set_mode(FAN_PWM_CHANNEL, 1, 1); //PWM_CHANNEL 0
	bcm2835_pwm_set_range(FAN_PWM_CHANNEL, 1024);//PWM_CHANNEL 0 Set Range to 1024

	bcm2835_pwm_set_mode(IR_PWM_CHANNEL, 1, 1);  //PWM_CHANNEL 1
	bcm2835_pwm_set_range(IR_PWM_CHANNEL, 1024);   //PWM_CHANNEL 1 Set Range to 1024

	bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_2);

	bcm2835_gpio_fsel(PIN_PRIMARY, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_ALT, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_MODE, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_RESET, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_REMOTE, BCM2835_GPIO_FSEL_INPT);
	
	bcm2835_gpio_set_pud(PIN_PRIMARY, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_ALT, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_MODE, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_RESET, BCM2835_GPIO_PUD_UP);
    bcm2835_gpio_set_pud(PIN_REMOTE, BCM2835_GPIO_PUD_UP);
}

int io_servo(bool servo_open) {

	static bool servo_request_previous = false;
	static int pulses = 0;

	if (servo_request_previous != servo_open) {
		pulses = 0;
		servo_request_previous = servo_open;
	}

	if (pulses < SERVO_NUM_PULSES) {
		pulses++;
		bcm2835_gpio_write(PIN_SERVO_SOFT_PWM, HIGH);
		uint64_t complete_time = bcm2835_st_read();
		if(servo_open) {
			complete_time += SERVO_OPEN_TIME;
			while(bcm2835_st_read() < complete_time) {
				;
			}
			bcm2835_gpio_write(PIN_SERVO_SOFT_PWM, LOW);
			return SERVO_OPEN_TIME/1000;
		} else {
			complete_time += SERVO_CLOSED_TIME;
			while(bcm2835_st_read() < complete_time) {
				;
			}
			bcm2835_gpio_write(PIN_SERVO_SOFT_PWM, LOW);
			return SERVO_CLOSED_TIME/1000;
		}

	}
	return 0;
}

int io_update(struct gun_struct *this_gun)
{
	bcm2835_pwm_set_data(FAN_PWM_CHANNEL, this_gun->fan_pwm);

	if (this_gun->state_duo < -1)	bcm2835_pwm_set_data(IR_PWM_CHANNEL, this_gun->ir_pwm);
	else							bcm2835_pwm_set_data(IR_PWM_CHANNEL, 0);

	//buckets for debounce
	static uint_fast8_t primary_bucket = 0;
	static uint_fast8_t alt_bucket = 0;
	static uint_fast8_t mode_bucket = 0;
	static uint_fast8_t reset_bucket = 0;
	static uint_fast8_t remote_bucket = 0;
	
	static uint32_t reset_hold_time = 0;

	uint8_t button = BUTTON_NONE;

	//BUTTON_RESET (lowest priority)
	if(bcm2835_gpio_lev(PIN_RESET) == 0) {
		if (reset_bucket < DEBOUNCE_COUNT) {
			reset_bucket++;
			if (reset_bucket == DEBOUNCE_COUNT) {
				reset_hold_time = millis();
				return BUTTON_RESET;
			}
		}
	} else {
		reset_hold_time = 0;
		reset_bucket = 0;
	}

	//Remote_button trigger
	bool remote_button = bcm2835_gpio_lev(PIN_REMOTE);
	
	if(remote_button == 0) {
		if (remote_bucket < DEBOUNCE_COUNT) 
			remote_bucket++;
	}
	

	//BUTTON_MODE_TOGGLE
	if(bcm2835_gpio_lev(PIN_MODE) == 0) {
		if (mode_bucket < DEBOUNCE_COUNT) {
			mode_bucket++;
			if (mode_bucket == DEBOUNCE_COUNT) button = BUTTON_MODE_TOGGLE;
		}
	} else {
		mode_bucket = 0;
	}

	//BUTTON_ALT_FIRE
	if(bcm2835_gpio_lev(PIN_ALT) == 0) {
		if (alt_bucket < DEBOUNCE_COUNT) {
			alt_bucket++;
			if (alt_bucket == DEBOUNCE_COUNT) button = BUTTON_ALT_FIRE;
		}
	} else {
		alt_bucket = 0;
	}

	//BUTTON_PRIMARY_FIRE
	if(bcm2835_gpio_lev(PIN_PRIMARY) == 0) {
		if (primary_bucket < DEBOUNCE_COUNT) {
			primary_bucket++;
			if (primary_bucket == DEBOUNCE_COUNT) button = BUTTON_PRIMARY_FIRE;
		}
	} else {
		primary_bucket = 0;
	}

	//BUTTON_LONG_RESET calculate countdown  (highest priority)
	if (reset_hold_time != 0) {
		if (millis() - reset_hold_time > RESET_HOLD_DELAY) {
			this_gun->reset_countdown = RESET_COUNTDOWN - ((millis() - reset_hold_time) - RESET_HOLD_DELAY);
			if (this_gun->reset_countdown < 0) {
				button = BUTTON_LONG_RESET;
			}
		} else {
			this_gun->reset_countdown = 0;
		}
	} else {
		this_gun->reset_countdown = 0;
	}
	
	// only clear remote bucket if nothing is pressed
	if (button == BUTTON_NONE && remote_button != 0){
		remote_bucket = 0;
	}
	
	if (remote_bucket == DEBOUNCE_COUNT){
		if (button == BUTTON_PRIMARY_FIRE)
		button = BUTTON_REMOTE_PRIMARY_FIRE;
		else if (button == BUTTON_ALT_FIRE)
		button = BUTTON_REMOTE_ALT_FIRE;
		else if (button == BUTTON_MODE_TOGGLE)
		button = BUTTON_REMOTE_MODE_TOGGLE;
		else if (button == BUTTON_RESET)
		button = BUTTON_REMOTE_RESET;
		else if (button == BUTTON_LONG_RESET)
		button = BUTTON_REMOTE_LONG_RESET;
	}
	
	return button;
}
