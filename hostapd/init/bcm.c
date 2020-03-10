#include <bcm2835.h>
//run this before launching hostapd on the pi4!
void main(void){
	bcm2835_init();
	bcm2835_gpio_fsel(12, BCM2835_GPIO_FSEL_ALT0); //PWM_CHANNEL 0
	bcm2835_gpio_fsel(13, BCM2835_GPIO_FSEL_ALT0);  //PWM_CHANNEL 1
	
	bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
	
	bcm2835_pwm_set_mode(12, 1, 1); //PWM_CHANNEL 0
	bcm2835_pwm_set_range(0, 1024);//PWM_CHANNEL 0 Set Range to 1024
	
	bcm2835_pwm_set_mode(13, 1, 1);  //PWM_CHANNEL 1
	bcm2835_pwm_set_range(1, 1024);   //PWM_CHANNEL 1 Set Range to 1024
}