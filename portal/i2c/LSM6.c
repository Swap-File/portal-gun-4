#include "LSM6.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <bcm2835.h>
#include <stdio.h>
// converted from https://github.com/pololu/lsm6-arduino/blob/master/LSM6.cpp

// Defines ////////////////////////////////////////////////////////////////

// The Arduino two-wire interface uses a 7-bit number for the address,
// and sets the last bit correctly based on reads and writes
#define DS33_SA0_HIGH_ADDRESS 0b1101011
#define DS33_SA0_LOW_ADDRESS  0b1101010

#define TEST_REG_ERROR -1

#define DS33_WHO_ID    0x69

enum deviceType _device = device_DS33; // chip type
uint8_t address = DS33_SA0_HIGH_ADDRESS;

static int testReg(char address, char reg)
{
	char temp;
	bcm2835_i2c_setSlaveAddress(address);
	bcm2835_i2c_write((char *)&reg,1);
	bcm2835_i2c_read(&temp,1);
	return temp;
}

static void writeReg(char reg, char value)
{
	char temp[2];
	temp[0] = reg;
	temp[1] = value;
	bcm2835_i2c_setSlaveAddress(address);
	bcm2835_i2c_write(temp,2);
}

// Public Methods //////////////////////////////////////////////////////////////

bool LSM6_init(enum deviceType device,enum sa0State sa0)
{  
	// perform auto-detection unless device type and SA0 state were both specified
	if (device == device_auto || sa0 == sa0_auto)
	{
		// check for LSM6DS33 if device is unidentified or was specified to be this type
		if (device == device_auto || device == device_DS33)
		{
			// check SA0 high address unless SA0 was specified to be low
			if (sa0 != sa0_low && testReg(DS33_SA0_HIGH_ADDRESS, WHO_AM_I) == DS33_WHO_ID)
			{
				sa0 = sa0_high;
				if (device == device_auto) { device = device_DS33; }
			}
			// check SA0 low address unless SA0 was specified to be high
			else if (sa0 != sa0_high && testReg(DS33_SA0_LOW_ADDRESS, WHO_AM_I) == DS33_WHO_ID)
			{
				sa0 = sa0_low;
				if (device == device_auto) { device = device_DS33; }
			}
		}

		// make sure device and SA0 were successfully detected; otherwise, indicate failure
		if (device == device_auto || sa0 == sa0_auto)
		{
			return false;
		}
	}

	_device = device;

	switch (device)
	{
	case device_auto:
	case device_DS33:
		address = (sa0 == sa0_high) ? DS33_SA0_HIGH_ADDRESS : DS33_SA0_LOW_ADDRESS;
		break;
	}

	return true;
}

/*
Enables the LSM6's accelerometer and gyro. Also:
- Sets sensor full scales (gain) to default power-on values, which are
+/- 2 g for accelerometer and 245 dps for gyro
- Selects 1.66 kHz (high performance) ODR (output data rate) for accelerometer
and 1.66 kHz (high performance) ODR for gyro. (These are the ODR settings for
which the electrical characteristics are specified in the datasheet.)
- Enables automatic increment of register address during multiple byte access
Note that this function will also reset other settings controlled by
the registers it writes to.
*/
void LSM6_enableDefault(void)
{
	if (_device == device_DS33)
	{
		//// LSM6DS33 gyro

		// ODR = 1000 (1.66 kHz (high performance))
		// FS_G = 11 (2000 dps)
		writeReg(CTRL2_G , 0b01011000 );

		// defaults
		writeReg(CTRL7_G, 0b00000000);

		//// LSM6DS33 accelerometer

		// ODR = 1000 (1.66 kHz (high performance))
		// FS_XL = 11 (8 g full scale)
		// BW_XL = 00 (400 Hz filter bandwidth)
		writeReg(CTRL1_XL, 0b01000011);

		//// common

		// IF_INC = 1 (automatically increment address register)
		writeReg(CTRL3_C, 0b01000100);
		
		printf("device_DS33\n");
	}
}



// Reads the 3 gyro channels and stores them in vector g
void LSM6_readGyro(int * gyro_data)
{
	char temp[6];
	temp[0] = OUTX_L_G;

	bcm2835_i2c_setSlaveAddress(address);
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,6);

	gyro_data[0] = (int16_t)(temp[0] | temp[1] << 8);
	gyro_data[1] = (int16_t)(temp[2] | temp[3] << 8);
	gyro_data[2] = (int16_t)(temp[4] | temp[5] << 8);
	
}
