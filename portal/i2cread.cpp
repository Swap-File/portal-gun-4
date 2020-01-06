#include "portal.h"
#include "i2cread.h"
#include <bcm2835.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <byteswap.h>
#include <math.h>
#include <stdlib.h>

static int i2c_accel[3];
static int i2c_adc[4];

#define accel_fd 0x19
#define adc_fd   0x48

static int adc_channel = 0;  //what channel we are working on
static bool adc_done = true; //is a conversion in progress?

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

void read_acclerometer(){
	
	 bcm2835_i2c_setSlaveAddress(accel_fd); 
	  
	//wiringPiI2CWrite(accel_fd, 0x80 | 0x28);

	char temp[6];
	temp[0] =  0x80 | 0x28;
	bcm2835_i2c_write(temp,1);
	
	//read(accel_fd, temp,6);
	bcm2835_i2c_read(temp,6);
	
	i2c_accel[0] = (int16_t)(temp[0] | temp[1] << 8);
	i2c_accel[1] = (int16_t)(temp[2] | temp[3] << 8);
	i2c_accel[2] = (int16_t)(temp[4] | temp[5] << 8);
	
}
void i2c_write16(uint16_t input){
	char temp[2];
	temp[0] = (uint8_t)(input>>8);
	temp[1] = ((uint8_t)(input & 0xFF));
	bcm2835_i2c_write(temp,2);
}

uint16_t i2c_read16(uint8_t reg) {
  char temp[2];
  temp[0] = reg;
  bcm2835_i2c_write(temp,1);
  bcm2835_i2c_read(temp,2);
  return ((temp[0] << 8) | temp[1]);  
}

void start_analog_read(){
	adc_done = false;
	uint16_t config = CONFIG_DEFAULT;

	// Setup the configuration register
	//	Set PGA/voltage range
	config &= ~CONFIG_PGA_MASK;
	config |= CONFIG_PGA_4_096V;  //set gain

	//	Set sample speed
	config &= ~CONFIG_DR_MASK;
	config |= CONFIG_DR_128SPS; //adc sample speed is ideally at least the accelerometer rate

	//	Set single-ended channel or differential mode
	config &= ~CONFIG_MUX_MASK ;

	switch (adc_channel){
	case 0: config |= CONFIG_MUX_SINGLE_0 ; break ;
	case 1: config |= CONFIG_MUX_SINGLE_1 ; break ;
	case 2: config |= CONFIG_MUX_SINGLE_2 ; break ;
	case 3: config |= CONFIG_MUX_SINGLE_3 ; break ;
	}

	//	Start a single conversion
	config |= CONFIG_OS_SINGLE;
	config = __bswap_16 (config);
	//wiringPiI2CWriteReg16 (adc_fd, 1, config);
	bcm2835_i2c_setSlaveAddress(adc_fd); 
	i2c_write16(config);
	
}

void finish_analog_read(){
	
	//int16_t result = wiringPiI2CReadReg16 (adc_fd, 0);
	bcm2835_i2c_setSlaveAddress(adc_fd); 
	int16_t result = i2c_read16(0);
	
	result = __bswap_16 (result);

	//Sometimes with a 0v input on a single-ended channel the internal 0v reference
	//can be higher than the input, so you get a negative result...
	
	if (result < 0)  result = 0;
	i2c_adc[adc_channel] = result;
	adc_channel++;
	if (adc_channel > 3) adc_channel = 0;
	
	adc_done = true;	
}

bool analog_read_ready(){
	//int16_t  result =  wiringPiI2CReadReg16(adc_fd, 1);
	bcm2835_i2c_setSlaveAddress(adc_fd); 
	int16_t result = i2c_read16(1);
	
	result = __bswap_16 (result);
	if ((result & CONFIG_OS_MASK) != 0) return true;
	return false;
}

bool gordon = true;
void i2creader_setup(void){
	if (getenv("GORDON")){
		gordon = true;
	}
	else if (getenv("CHELL")){
		gordon = false;
	}
	//accelerometer start
	//accel_fd =  wiringPiI2CSetup (0x19);

	//adc start
	//adc_fd =  wiringPiI2CSetup (0x48);
	bcm2835_i2c_begin();
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500); //100hz
	bcm2835_i2c_setSlaveAddress(accel_fd); 

	char temp[2];
	//// LSM303DLHC Accelerometer
	// ODR = 0100 (50 Hz ODR)
	// LPen = 0 (normal mode)
	// Zen = Yen = Xen = 1 (all axes enabled)

	temp[0] = CTRL_REG1_A;
	temp[1] = 0b01010111;
	//wiringPiI2CWriteReg8(accel_fd,CTRL_REG1_A, 0b01010111);
	 bcm2835_i2c_write(temp,2);
	// FS = 10 (8 g full scale)
	// HR = 1 (high resolution enable)
	
	temp[0] = CTRL_REG4_A;
	temp[1] = 0b00101000;
	//wiringPiI2CWriteReg8(accel_fd,temp, 0b00101000);
	 bcm2835_i2c_write(temp,2);
	 
}

void i2creader_update(this_gun_struct& this_gun){
	//uint32_t start_time = micros();
	
	if (!adc_done){
		if(analog_read_ready()) finish_analog_read();
	}
	if (adc_done) start_analog_read();
	
	read_acclerometer();
	
	//printf("Took %d microseconds!\n",micros() - start_time);	
	
	this_gun.accel[0] = i2c_accel[0];
	this_gun.accel[1] = i2c_accel[1];
	this_gun.accel[2] = i2c_accel[2];
	
	if (gordon){
	this_gun.battery_level_pretty = this_gun.battery_level_pretty * .9 + .1 *(float)i2c_adc[1] * 0.00074;
	}else{
	this_gun.battery_level_pretty = this_gun.battery_level_pretty * .9 + .1 *(float)i2c_adc[1] * 0.00072;
	}
	//21600 = 16.1 v  16000  = 12.1v
	this_gun.temperature_pretty =temperature_reading(i2c_adc[0]);
}


