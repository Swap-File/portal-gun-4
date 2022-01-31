#include "i2c.h"
#include <stdio.h>
#include <math.h>
#include <bcm2835.h>
#include "i2c/LSM6.h"
#include "i2c/sgtl5000.h"
#include "i2c/ads1115.h"

static float temp_conversion(int input,int offset)
{
	float R = (10000.0 + (float)offset) / (26000.0/((float)input) - 1.0);
	const float B = 3428.0; // Thermistor constant from thermistor datasheet
	const float R0 = 10000.0; // Resistance of the thermistor being used
	const float t0 = 273.15; // 0 deg C in K
	const float t25 = 298.15; //t25 = t0 + 25.0; // 25 deg C in K
	// Steinhart-Hart equation
	float inv_T = 1/t25 + 1/B * log(R/R0);
	float T = (1/inv_T - t0) * 1; //Adjust 1 here
	return T * 9.0 / 5.0 + 32.0; //Convert C to F
}

void calculate_offset(struct gun_struct *this_gun, int * gyro_data)
{
	const int WIDTH = 180;
	const int GYRO_DEADSPOT = 500;
	const float GYRO_BLENDING = 0.8;

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
	this_gun->particle_magnitude = sqrt(gyro_smoothed[0] * gyro_smoothed[0] + gyro_smoothed[2] * gyro_smoothed[2]);
	if (this_gun->particle_magnitude > GYRO_DEADSPOT) {

		float angle_target = atan2(gyro_smoothed[0],gyro_smoothed[2]);
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

void i2c_init(void)
{

	bcm2835_i2c_begin();
	//speed is already set to 100khz, setting it here can mess with other peripherals
	bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_2500); //BCM2835_I2C_CLOCK_DIVIDER_2500 = 100khz

	// Gyro Init
	LSM6_init(device_DS33,sa0_high);
	LSM6_enableDefault();

	// ads1115 setup
	ads1115_setup(0x48,ADS1115_GAIN_4,ADS1115_DR_128);

	// sgtl5000 & alsamixer
	sgtl5000_audioPreProcessorEnable();
	sgtl5000_autoVolumeEnable();
	// maxGain (0,1,2) lbiResponse (0,1,2,3) hardLimit (0,1)
	// threshold (0 means limit only)  attack (db) decay (db)
	sgtl5000_autoVolumeControl(0,1,1,0,10,1);
	system("/home/pi/portal/i2c/sgtl5000.sh");
	sgtl5000_audioPreProcessorEnable();
	sgtl5000_autoVolumeEnable();

}

void i2c_update(struct gun_struct *this_gun)
{
	/* read accel every update call for smooth data */
	int gyro_data[3];
	LSM6_readGyro(gyro_data);
	calculate_offset(this_gun,gyro_data);

	int * adc_data = ads1115_update();

	/* old battery calibration:  21600 = 16.1v  16000 = 12.1v */
	static float current = 0;
	if(this_gun->gordon) {
		this_gun->temperature_pretty = temp_conversion(adc_data[0],1500);
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)(adc_data[3]) * 0.00075;
		current = current * .9 + .1 *(float)adc_data[2] * 0.00075 * .166666;
		this_gun->current_pretty = (2.65 - current) / 0.185;   //.185 is fixed
	} else {
		this_gun->temperature_pretty = temp_conversion(adc_data[0],1500);
		this_gun->battery_level_pretty = this_gun->battery_level_pretty * .9 + .1 *(float)(adc_data[3]) * 0.000735;
		current = current * .9 + .1 *(float)(adc_data[2]) * 0.00075 * .166666;
		this_gun->current_pretty = (2.63 - current) / 0.185;  //.185 is fixed
	}
}
