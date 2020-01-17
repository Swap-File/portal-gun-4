#include "i2cread.h"
#include <stdio.h>
#include <math.h>
#include <bcm2835.h>

#define ACCEL_ADDR 0x19
#define ADC_ADDR   0x48

static int adc_channel = 0; //what channel we are working on
static int i2c_adc[4];      //channel stored data

float temperature_reading(int input){
	float R =  10000.0 / (26000.0/((float)input) - 1.0);
    #define B 3428.0 //# Thermistor constant from thermistor datasheet
    #define R0 10000.0 //# Resistance of the thermistor being used
    #define t0 273.15 //# 0 deg C in K
    #define t25 298.15 //t25 = t0 + 25.0; //# 25 deg C in K
    //# Steinhart-Hart equation
    float inv_T = 1/t25 + 1/B * log(R/R0);
    float T = (1/inv_T - t0) * 1; //adjust 1 here
    return T * 9.0 / 5.0 + 32.0; //# Convert C to F
}

void read_acclerometer(int accel[]){
	
	char temp[6];
	temp[0] = 0x80 | 0x28;
	
	bcm2835_i2c_setSlaveAddress(ACCEL_ADDR); 	
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,6);
	
	accel[0] = (temp[0] | temp[1] << 8);
	accel[1] = (temp[2] | temp[3] << 8);
	accel[2] = (temp[4] | temp[5] << 8);
	//printf("%d %d %d\n",i2c_accel[0],i2c_accel[1],i2c_accel[2]);
}

int16_t i2c_read16(uint8_t reg) {
  char temp[2];
  temp[0] = reg;
  bcm2835_i2c_write(temp,1);
  bcm2835_i2c_read(temp,2);
  return (temp[1] | temp[0] << 8);
}

void start_analog_read(){
	uint16_t config = CONFIG_DEFAULT;

	//Setup the configuration register
	//Set PGA/voltage range
	config &= ~CONFIG_PGA_MASK;
	config |= CONFIG_PGA_4_096V;  //set gain

	//Set sample speed
	config &= ~CONFIG_DR_MASK;
	config |= CONFIG_DR_128SPS; //adc sample speed is ideally at least the accelerometer rate

	//Set single-ended channel or differential mode
	config &= ~CONFIG_MUX_MASK ;

	switch (adc_channel){
	case 0: config |= CONFIG_MUX_SINGLE_0; break;
	case 1: config |= CONFIG_MUX_SINGLE_1; break;
	case 2: config |= CONFIG_MUX_SINGLE_2; break;
	case 3: config |= CONFIG_MUX_SINGLE_3; break;
	}

	//Start a single conversion
	config |= CONFIG_OS_SINGLE;
	
	char temp[3];
	temp[0] = 1;
	temp[1] = (config >> 8);
	temp[2] = (config & 0xFF);
	
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	bcm2835_i2c_write(temp,3);
}

void finish_analog_read(){
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	int16_t result = i2c_read16(0);
	
	//Sometimes with a 0v input on a single-ended channel the internal 0v reference
	//can be higher than the input, so you get a negative result...
	
	if (result < 0)  result = 0;
	i2c_adc[adc_channel++] = result;
	if (adc_channel > 3) adc_channel = 0;
}

bool analog_read_ready(){
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	int16_t result = i2c_read16(1);

	if ((result & CONFIG_OS_MASK) != 0) return true;
	return false;
}

void i2creader_setup(void){

	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500); //100hz
	bcm2835_i2c_setSlaveAddress(ACCEL_ADDR); 

	char temp[2];
	
	//// LSM303DLHC Accelerometer
	// ODR = 0100 (50 Hz ODR)
	// LPen = 0 (normal mode)
	// Zen = Yen = Xen = 1 (all axes enabled)
	temp[0] = CTRL_REG1_A;
	temp[1] = 0x57;
	bcm2835_i2c_write(temp,2);
	 
	// FS = 10 (8 g full scale)
	// HR = 1 (high resolution enable)
	temp[0] = CTRL_REG4_A;
	temp[1] = 0x28;
	bcm2835_i2c_write(temp,2);
}

void i2creader_update(struct gun_struct *this_gun){
	static bool adc_busy = false; //is a conversion in progress?
	
	//only do one ADC operation per tick to ensure even function return time
	if (adc_busy){
		if(analog_read_ready()){  //check elapsed time instead?  should never be not ready by the time it's called again
			finish_analog_read();
			adc_busy = false;
		}else{
			printf("Waiting on ADC....\n"); //this should never happen since the main loop delays long enough
		}
	}
	else{
		adc_busy = true;
		start_analog_read();
	}
	
	//read the IMU every tick for smooth effects
	read_acclerometer(this_gun->accel);
	
	if (this_gun->gordon){  //Is this still needed?  Check calibration.
	this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)i2c_adc[1] * 0.00074;
	}else{
	this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)i2c_adc[1] * 0.00072;
	}
	//21600 = 16.1 v  16000  = 12.1v
	this_gun->temperature_pretty =temperature_reading(i2c_adc[0]);
}


