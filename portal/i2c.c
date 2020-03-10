#include "i2c.h"
#include <stdio.h>
#include <math.h>
#include <bcm2835.h>

#define GYRO_ADDR  0x6B
#define ADC_ADDR   0x48

static int adc_active_channel = 0; //what channel we are working on
static int adc_data[4];
static int gyro_data[3];

static float temp_conversion(int input,int offset)
{
	float R = (10000.0 + (float)offset) / (26000.0/((float)input) - 1.0);
	#define B 3428.0 // Thermistor constant from thermistor datasheet
	#define R0 10000.0 // Resistance of the thermistor being used
	#define t0 273.15 // 0 deg C in K
	#define t25 298.15 //t25 = t0 + 25.0; // 25 deg C in K
	// Steinhart-Hart equation
	float inv_T = 1/t25 + 1/B * log(R/R0);
	float T = (1/inv_T - t0) * 1; //Adjust 1 here
	return T * 9.0 / 5.0 + 32.0; //Convert C to F
}

static void i2c_read_gyro()
{
	char temp[6];
	temp[0] = 0x80 | L3G_OUT_X_L;
	
	bcm2835_i2c_setSlaveAddress(GYRO_ADDR); 	
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,6);
	
	gyro_data[0] = (int16_t)(temp[0] | (uint16_t)temp[1] << 8);
	gyro_data[1] = (int16_t)(temp[2] | (uint16_t)temp[3] << 8);
	gyro_data[2] = (int16_t)(temp[4] | (uint16_t)temp[5] << 8);
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
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500); //BCM2835_I2C_CLOCK_DIVIDER_2500 = 100khz 
	bcm2835_i2c_setSlaveAddress(GYRO_ADDR); 

	char temp[2];
	
	// Gyro Init
	// Normal power mode, all axes enabled
	temp[0] = L3G_CTRL_REG1;
	temp[1] = 0b00001111;
	bcm2835_i2c_write(temp,2);
	
    // 2000 dps full scale
	temp[0] = L3G_CTRL_REG4;
	temp[1] = 0b00100000;
	bcm2835_i2c_write(temp,2);
}

void calculate_offset(struct gun_struct *this_gun)
{ 
	#define WIDTH 180
	#define GYRO_DEADSPOT 500
	#define GYRO_BLENDING 0.8

	//decay offsets
	for (int i = 0; i < 720; i++) this_gun->particle_offset[i] *= .98;
	
	//remap x, y, and z
	float gyro[3];
	gyro[0] = gyro_data[0];
	gyro[1] = gyro_data[1];
	gyro[2] = gyro_data[2];

	static float gyro_smoothed[3];
	
	//filter x and y - adjust so snap-back disappears
	for (int i = 0; i < 3; i++)
	gyro_smoothed[i] = gyro_smoothed[i] * GYRO_BLENDING + (1.0-GYRO_BLENDING) * gyro[i];
	
	//find magnitude
	this_gun->particle_magnitude = sqrt( gyro_smoothed[1] * gyro_smoothed[1] + gyro_smoothed[2] * gyro_smoothed[2]);
	if (this_gun->particle_magnitude > GYRO_DEADSPOT){
	
		float angle_target = atan2(gyro_smoothed[1],gyro_smoothed[2]);
		int array_target = (int)(720.0 * angle_target/(2.0*M_PI) + 720.0) % 720; 
		
		for (int i = 0; i <= 360; i++) { //go over half the circle (it's mirrored) plus one (to cover the unmirrored pixel at i=0)
			
			float value = 0.0;
			
			if (i <= WIDTH)
				value = (cos(i * M_PI/WIDTH)+ 1.0) / 2.0; //remap -1 to 1 into 0 to 1

			int right_target = (array_target + i + 720) % 720;
			int left_target = (array_target - i + 720) % 720;

			if (this_gun->particle_offset[right_target] < value)
				this_gun->particle_offset[right_target] = value;	

			if (this_gun->particle_offset[left_target] < value)
				this_gun->particle_offset[left_target] = value;

		}
	}
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
			//printf("Waiting on ADC....\n"); 
		}
	} else {
		adc_busy = true;
		i2c_adc_start();
	}
	
	/* read accel every update call for smooth data */
	i2c_read_gyro();
	calculate_offset(this_gun);
	
	/* old battery calibration:  21600 = 16.1v  16000 = 12.1v */	
	static float current = 0;	
	if(this_gun->gordon){
		this_gun->temperature_pretty = temp_conversion(adc_data[0],1500);
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)adc_data[2] * 0.00075;
		current = current * .9 + .1 *(float)adc_data[3] * 0.00075 * .166666;
		this_gun->current_pretty = (2.65 - current) / 0.185;   //.185 is fixed
	}else{
		this_gun->temperature_pretty = temp_conversion(adc_data[0],600);
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)adc_data[2] * 0.000735;
		current = current * .9 + .1 *(float)adc_data[3] * 0.00075 * .166666;
		this_gun->current_pretty = (2.63 - current) / 0.185;  //.185 is fixed
	}	
}
