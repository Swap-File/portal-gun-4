#include "L3G.h"
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <bcm2835.h>

// converted from https://github.com/pololu/l3g-arduino/blob/master/L3G.cpp

// Defines ////////////////////////////////////////////////////////////////

// The Arduino two-wire interface uses a 7-bit number for the address,
// and sets the last bit correctly based on reads and writes
#define D20_SA0_HIGH_ADDRESS      0b1101011 // also applies to D20H
#define D20_SA0_LOW_ADDRESS       0b1101010 // also applies to D20H
#define L3G4200D_SA0_HIGH_ADDRESS 0b1101001
#define L3G4200D_SA0_LOW_ADDRESS  0b1101000

#define TEST_REG_ERROR -1

#define D20H_WHO_ID     0xD7
#define D20_WHO_ID      0xD4
#define L3G4200D_WHO_ID 0xD3

enum deviceType _device; // chip type (D20H, D20, or 4200D)
char address;

static int testReg(char address, char reg)
{
	char temp;
	bcm2835_i2c_setSlaveAddress(address); 
	bcm2835_i2c_write((char *)&reg,1);
	bcm2835_i2c_read(&temp,1);
	return temp;
}

// Writes a gyro register
static void writeReg(char reg, char value)
{
	char temp[2];
	temp[0] = reg;
	temp[1] = value;
	bcm2835_i2c_setSlaveAddress(address);
	bcm2835_i2c_write(temp,2);
}

bool L3G_init(enum deviceType device,enum sa0State sa0)
{
	int id;

	// perform auto-detection unless device type and SA0 state were both specified
	if (device == device_auto || sa0 == sa0_auto)
	{
		// check for L3GD20H, D20 if device is unidentified or was specified to be one of these types
		if (device == device_auto || device == device_D20H || device == device_D20)
		{
			// check SA0 high address unless SA0 was specified to be low
			if (sa0 != sa0_low && (id = testReg(D20_SA0_HIGH_ADDRESS, WHO_AM_I)) != TEST_REG_ERROR)
			{
				// device responds to address 1101011; it's a D20H or D20 with SA0 high     
				sa0 = sa0_high;
				if (device == device_auto)
				{
					// use ID from WHO_AM_I register to determine device type
					device = (id == D20H_WHO_ID) ? device_D20H : device_D20;
				}
			}
			// check SA0 low address unless SA0 was specified to be high
			else if (sa0 != sa0_high && (id = testReg(D20_SA0_LOW_ADDRESS, WHO_AM_I)) != TEST_REG_ERROR)
			{
				// device responds to address 1101010; it's a D20H or D20 with SA0 low      
				sa0 = sa0_low;
				if (device == device_auto)
				{
					// use ID from WHO_AM_I register to determine device type
					device = (id == D20H_WHO_ID) ? device_D20H : device_D20;
				}
			}
		}
		
		// check for L3G4200D if device is still unidentified or was specified to be this type
		if (device == device_auto || device == device_4200D)
		{
			if (sa0 != sa0_low && testReg(L3G4200D_SA0_HIGH_ADDRESS, WHO_AM_I) == L3G4200D_WHO_ID)
			{
				// device responds to address 1101001; it's a 4200D with SA0 high
				device = device_4200D;
				sa0 = sa0_high;
			}
			else if (sa0 != sa0_high && testReg(L3G4200D_SA0_LOW_ADDRESS, WHO_AM_I) == L3G4200D_WHO_ID)
			{
				// device responds to address 1101000; it's a 4200D with SA0 low
				device = device_4200D;
				sa0 = sa0_low;
			}
		}
		
		// make sure device and SA0 were successfully detected; otherwise, indicate failure
		if (device == device_auto || sa0 == sa0_auto)
		{
			return false;
		}
	}

	_device = device;

	// set device address
	switch (device)
	{
	case device_D20H:
	case device_D20:
		address = (sa0 == sa0_high) ? D20_SA0_HIGH_ADDRESS : D20_SA0_LOW_ADDRESS;
		break;

	case device_4200D:
		address = (sa0 == sa0_high) ? L3G4200D_SA0_HIGH_ADDRESS : L3G4200D_SA0_LOW_ADDRESS;
		break;
	case device_auto:
		break;
	}

	return true;
}

/*
Enables the L3G's gyro. Also:
- Sets gyro full scale (gain) to default power-on value of +/- 250 dps
(specified as +/- 245 dps for L3GD20H).
- Selects 200 Hz ODR (output data rate). (Exact rate is specified as 189.4 Hz
for L3GD20H and 190 Hz for L3GD20.)
Note that this function will also reset other settings controlled by
the registers it writes to.
*/
void L3G_enableDefault(void)
{
	if (_device == device_D20H)
	{
		// 0x00 = 0b00000000
		// Low_ODR = 0 (low speed ODR disabled)
		writeReg(LOW_ODR, 0x00);
	}

	// 0x20 = 0b00100000
	// FS = 10 (+/- 2000 dps full scale)
	writeReg(CTRL_REG4, 0x20);

	// 0x0F = 0b00001111
	// DR = 00 (100 Hz ODR); BW = 00 (12.5 Hz bandwidth); PD = 1 (normal mode); Zen = Yen = Xen = 1 (all axes enabled)
	writeReg(CTRL_REG1, 0x0F);
}

// Reads the 3 gyro channels and stores them in vector g
void L3G_read(int * gyro_data)
{
	char temp[6];
	temp[0] = OUT_X_L | (1 << 7);

	bcm2835_i2c_setSlaveAddress(address); 	
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,6);
	
	gyro_data[0] = (int16_t)(temp[0] | (uint16_t)temp[1] << 8);
	gyro_data[1] = (int16_t)(temp[2] | (uint16_t)temp[3] << 8);
	gyro_data[2] = (int16_t)(temp[4] | (uint16_t)temp[5] << 8);

}
