#include "i2c.h"
#include <stdio.h>
#include <math.h>
#include <bcm2835.h>

#define ACCEL_ADDR 0x19
#define ADC_ADDR   0x48

static int adc_active_channel = 0; //what channel we are working on
static int adc_data[4];
	
static float temp_conversion(int input)
{
	float R =  10000.0 / (26000.0/((float)input) - 1.0);
	#define B 3428.0 // Thermistor constant from thermistor datasheet
	#define R0 10000.0 // Resistance of the thermistor being used
	#define t0 273.15 // 0 deg C in K
	#define t25 298.15 //t25 = t0 + 25.0; // 25 deg C in K
	// Steinhart-Hart equation
	float inv_T = 1/t25 + 1/B * log(R/R0);
	float T = (1/inv_T - t0) * 1; //Adjust 1 here
	return T * 9.0 / 5.0 + 32.0; //Convert C to F
}

static void i2c_read_accel(int *accel)
{
	char temp[6];
	temp[0] = 0x80 | 0x28;
	
	bcm2835_i2c_setSlaveAddress(ACCEL_ADDR); 	
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,6);
	
	accel[0] = temp[0] | temp[1] << 8;
	accel[1] = temp[2] | temp[3] << 8;
	accel[2] = temp[4] | temp[5] << 8;
}

static int16_t i2c_read16(uint8_t reg)
{
	char temp[2];
	temp[0] = reg;
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,2);
	return temp[1] | temp[0] << 8;
}

static void i2c_adc_start()
{
	uint16_t config = CONFIG_DEFAULT;

	/* set range */ 
	config &= ~CONFIG_PGA_MASK;
	config |= CONFIG_PGA_4_096V; 

	/* set sample rate */
	/* must be as fast as the accelerometer rate's reading cadence */ 
	config &= ~CONFIG_DR_MASK;
	config |= CONFIG_DR_128SPS;

	/* setup a single channel conversion */
	config &= ~CONFIG_MUX_MASK ;

	switch (adc_active_channel){
	case 0: config |= CONFIG_MUX_SINGLE_0; break;
	case 1: config |= CONFIG_MUX_SINGLE_1; break;
	case 2: config |= CONFIG_MUX_SINGLE_2; break;
	case 3: config |= CONFIG_MUX_SINGLE_3; break;
	}

	/* start a single conversion */
	config |= CONFIG_OS_SINGLE;
	
	char temp[3];
	temp[0] = 1;
	temp[1] = (config >> 8);
	temp[2] = (config & 0xFF);
	
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	bcm2835_i2c_write(temp,3);
}

static void i2c_adc_finish()
{
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	int16_t result = i2c_read16(0);
	
	//Sometimes with a 0v input on a single-ended channel the internal 0v reference
	//can be higher than the input, so you get a negative result...
	if (result < 0) result = 0;
	
	/* save data and move to next channel */
	adc_data[adc_active_channel++] = result;
	if (adc_active_channel > 3) adc_active_channel = 0;
}

static bool i2c_adc_ready()
{
	bcm2835_i2c_setSlaveAddress(ADC_ADDR); 
	int16_t result = i2c_read16(1);

	if ((result & CONFIG_OS_MASK) != 0) return true;
	return false;
}

void i2c_init(void)
{
	bcm2835_i2c_begin();
	//speed is already set to 100khz, setting it here can mess with other peripherals 
	//bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500); //BCM2835_I2C_CLOCK_DIVIDER_2500 = 100khz 
	bcm2835_i2c_setSlaveAddress(ACCEL_ADDR); 

	char temp[2];
	
	// LSM303DLHC Accelerometer
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

void i2c_update(struct gun_struct *this_gun)
{
	static bool adc_busy = false;

	if (adc_busy) { 
		if (i2c_adc_ready()){  //superfulous
			i2c_adc_finish();
			adc_busy = false;
		} else {
			/* under normal i2c update cadence calling this should never happen */
			printf("Waiting on ADC....\n"); 
		}
	} else {
		adc_busy = true;
		i2c_adc_start();
	}
	
	/* read accel every update call for smooth data */
	i2c_read_accel(this_gun->accel);
	
	/* old battery calibration:  21600 = 16.1v  16000 = 12.1v */	
	if (this_gun->gordon)  //update these correction values
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)adc_data[1] * 0.00074;
	else
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)adc_data[1] * 0.00072;
	
	this_gun->temperature_pretty = temp_conversion(adc_data[0]);
}
