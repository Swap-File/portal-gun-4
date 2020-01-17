#include "io.h"
#include <bcm2835.h>

void io_init(void)
{
	bcm2835_gpio_fsel(PIN_FAN_PWM, BCM2835_GPIO_FSEL_ALT0); //PWM_CHANNEL 0
	bcm2835_gpio_fsel(PIN_IR_PWM, BCM2835_GPIO_FSEL_ALT0);  //PWM_CHANNEL 1
	
	bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
	
	bcm2835_pwm_set_mode(FAN_PWM_CHANNEL, 1, 1); //PWM_CHANNEL 0
	bcm2835_pwm_set_range(FAN_PWM_CHANNEL, 1024);//PWM_CHANNEL 0 Set Range to 1024
	
	bcm2835_pwm_set_mode(IR_PWM_CHANNEL, 1, 1);  //PWM_CHANNEL 1
	bcm2835_pwm_set_range(IR_PWM_CHANNEL, 1024);   //PWM_CHANNEL 1 Set Range to 1024
	
	bcm2835_gpio_fsel(PIN_PRIMARY, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_ALT, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_MODE, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_RESET, BCM2835_GPIO_FSEL_INPT);
	
	bcm2835_gpio_set_pud(PIN_PRIMARY, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_ALT, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_MODE, BCM2835_GPIO_PUD_UP);
	bcm2835_gpio_set_pud(PIN_RESET, BCM2835_GPIO_PUD_UP);
}

int io_update(const struct gun_struct *this_gun)
{
	bcm2835_pwm_set_data(FAN_PWM_CHANNEL, this_gun->fan_pwm);
	
	if (this_gun->state_duo < 0)	bcm2835_pwm_set_data(IR_PWM_CHANNEL, this_gun->ir_pwm);
	else							bcm2835_pwm_set_data(IR_PWM_CHANNEL, 0);
	
	static uint_fast8_t primary_bucket = 0;
	static uint_fast8_t alt_bucket = 0;
	static uint_fast8_t mode_bucket = 0;
	static uint_fast8_t reset_bucket = 0;
	
	static bool primary_cleared = true;
	static bool alt_cleared = true;
	static bool mode_cleared = true;
	static bool reset_cleared = true;
	
	#define DEBOUNCE_MAX 3
	//basic bucket debounce
	if(bcm2835_gpio_lev(PIN_PRIMARY) == 0) {
		if (primary_bucket < DEBOUNCE_MAX) {
			primary_bucket++;
			if (primary_bucket == DEBOUNCE_MAX && primary_cleared) return BUTTON_PRIMARY_FIRE;
		}
	} else {
		if (primary_bucket > 0) {
			primary_bucket--;
			if (primary_bucket == 0) primary_cleared = true;
		}
	}
	
	if(bcm2835_gpio_lev(PIN_ALT) == 0) {
		if (alt_bucket < DEBOUNCE_MAX) {
			alt_bucket++;
			if (alt_bucket == DEBOUNCE_MAX && alt_cleared) return BUTTON_ALT_FIRE;
		}
	} else {
		if (alt_bucket > 0) {
			alt_bucket--;
			if (alt_bucket == 0) alt_cleared = true;
		}
	}
	
	if(bcm2835_gpio_lev(PIN_MODE) == 0) {
		if (mode_bucket < DEBOUNCE_MAX) {
			mode_bucket++;
			if (mode_bucket == DEBOUNCE_MAX && mode_cleared) return BUTTON_MODE_TOGGLE;
		}
	} else {
		if (mode_bucket > 0){
			mode_bucket--;
			if (mode_bucket == 0) mode_cleared = true;
		}
	}
	
	if(bcm2835_gpio_lev(PIN_RESET) == 0) {
		if (reset_bucket < DEBOUNCE_MAX) {
			reset_bucket++;
			if (reset_bucket == DEBOUNCE_MAX && reset_cleared) return BUTTON_RESET;
		}
	} else {
		if (reset_bucket > 0) {
			reset_bucket--;
			if (reset_bucket == 0) reset_cleared = true;
		}
	}
	
	return BUTTON_NONE;	
}
