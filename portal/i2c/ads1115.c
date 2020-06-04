/*
* ads1115.c:
*	Extend wiringPi with the ADS1115 I2C 16-bit ADC
*	Copyright (c) 2016 Gordon Henderson
***********************************************************************
* This file is part of wiringPi:
*	https://projects.drogon.net/raspberry-pi/wiringpi/
*
*    wiringPi is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Lesser General Public License as
*    published by the Free Software Foundation, either version 3 of the
*    License, or (at your option) any later version.
*
*    wiringPi is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Lesser General Public License for more details.
*
*    You should have received a copy of the GNU Lesser General Public
*    License along with wiringPi.
*    If not, see <http://www.gnu.org/licenses/>.
***********************************************************************
*/

/*
*********************************************************************************
* We're going to work in a hybrid mode to fit in with the wiringPi way of
* doing things, so there will be 4 analog pin which read the 4 single-ended
* channels as usual, also some fake digitalOutputs - these are the control
* registers that allow the user to put it into single/diff mode, set the
* gain and data rates.
*********************************************************************************
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <bcm2835.h>
#include "ads1115.h"

// Bits in the config register (it's a 16-bit register)

#define	CONFIG_OS_MASK		(0x8000)	// Operational Status Register
#define	CONFIG_OS_SINGLE	(0x8000)	// Write - Starts a single-conversion
// Read    1 = Conversion complete

// The multiplexor

#define	CONFIG_MUX_MASK		(0x7000)

// Differential modes

#define	CONFIG_MUX_DIFF_0_1	(0x0000)	// Pos = AIN0, Neg = AIN1 (default)
#define	CONFIG_MUX_DIFF_0_3	(0x1000)	// Pos = AIN0, Neg = AIN3
#define	CONFIG_MUX_DIFF_1_3	(0x2000)	// Pos = AIN1, Neg = AIN3
#define	CONFIG_MUX_DIFF_2_3	(0x3000)	// Pos = AIN2, Neg = AIN3 (2nd differential channel)

// Single-ended modes

#define	CONFIG_MUX_SINGLE_0	(0x4000)	// AIN0
#define	CONFIG_MUX_SINGLE_1	(0x5000)	// AIN1
#define	CONFIG_MUX_SINGLE_2	(0x6000)	// AIN2
#define	CONFIG_MUX_SINGLE_3	(0x7000)	// AIN3

// Programmable Gain Amplifier

#define	CONFIG_PGA_MASK		(0x0E00)
#define	CONFIG_PGA_6_144V	(0x0000)	// +/-6.144V range = Gain 2/3
#define	CONFIG_PGA_4_096V	(0x0200)	// +/-4.096V range = Gain 1
#define	CONFIG_PGA_2_048V	(0x0400)	// +/-2.048V range = Gain 2 (default)
#define	CONFIG_PGA_1_024V	(0x0600)	// +/-1.024V range = Gain 4
#define	CONFIG_PGA_0_512V	(0x0800)	// +/-0.512V range = Gain 8
#define	CONFIG_PGA_0_256V	(0x0A00)	// +/-0.256V range = Gain 16

#define	CONFIG_MODE		(0x0100)	// 0 is continuous, 1 is single-shot (default)

// Data Rate

#define	CONFIG_DR_MASK		(0x00E0)
#define	CONFIG_DR_8SPS		(0x0000)	//   8 samples per second
#define	CONFIG_DR_16SPS		(0x0020)	//  16 samples per second
#define	CONFIG_DR_32SPS		(0x0040)	//  32 samples per second
#define	CONFIG_DR_64SPS		(0x0060)	//  64 samples per second
#define	CONFIG_DR_128SPS	(0x0080)	// 128 samples per second (default)
#define	CONFIG_DR_475SPS	(0x00A0)	// 475 samples per second
#define	CONFIG_DR_860SPS	(0x00C0)	// 860 samples per second

// Comparator mode

#define	CONFIG_CMODE_MASK	(0x0010)
#define	CONFIG_CMODE_TRAD	(0x0000)	// Traditional comparator with hysteresis (default)
#define	CONFIG_CMODE_WINDOW	(0x0010)	// Window comparator

// Comparator polarity - the polarity of the output alert/rdy pin

#define	CONFIG_CPOL_MASK	(0x0008)
#define	CONFIG_CPOL_ACTVLOW	(0x0000)	// Active low (default)
#define	CONFIG_CPOL_ACTVHI	(0x0008)	// Active high

// Latching comparator - does the alert/rdy pin latch

#define	CONFIG_CLAT_MASK	(0x0004)
#define	CONFIG_CLAT_NONLAT	(0x0000)	// Non-latching comparator (default)
#define	CONFIG_CLAT_LATCH	(0x0004)	// Latching comparator

// Comparitor queue

#define	CONFIG_CQUE_MASK	(0x0003)
#define	CONFIG_CQUE_1CONV	(0x0000)	// Assert after one conversions
#define	CONFIG_CQUE_2CONV	(0x0001)	// Assert after two conversions
#define	CONFIG_CQUE_4CONV	(0x0002)	// Assert after four conversions
#define	CONFIG_CQUE_NONE	(0x0003)	// Disable the comparator (default)

#define	CONFIG_DEFAULT		(0x8583)	// From the datasheet



static uint16_t global_gain;	// Gain in global_gain
static uint16_t global_dr;	// Samples/sec in global_dr
static uint8_t i2cAddr;

static const uint16_t dataRates [8] =
{
	CONFIG_DR_8SPS, CONFIG_DR_16SPS, CONFIG_DR_32SPS, CONFIG_DR_64SPS, CONFIG_DR_128SPS, CONFIG_DR_475SPS, CONFIG_DR_860SPS
} ;

static const uint16_t gains [6] =
{
	CONFIG_PGA_6_144V, CONFIG_PGA_4_096V, CONFIG_PGA_2_048V, CONFIG_PGA_1_024V, CONFIG_PGA_0_512V, CONFIG_PGA_0_256V
} ;


/*
* analogRead:
*	Pin is the channel to sample on the device.
*	Channels 0-3 are single ended inputs,
*	channels 4-7 are the various differential combinations.
*********************************************************************************
*/

static void adc_start (int chan)
{
	uint16_t config = CONFIG_DEFAULT;

	// Setup the configuration register

	//	Set PGA/voltage range

	config &= ~CONFIG_PGA_MASK;
	config |= global_gain;

	//	Set sample speed

	config &= ~CONFIG_DR_MASK;
	config |= global_dr;

	//	Set single-ended channel or differential mode

	config &= ~CONFIG_MUX_MASK;

	switch (chan)
	{
	case 0: config |= CONFIG_MUX_SINGLE_0 ; break ;
	case 1: config |= CONFIG_MUX_SINGLE_1 ; break ;
	case 2: config |= CONFIG_MUX_SINGLE_2 ; break ;
	case 3: config |= CONFIG_MUX_SINGLE_3 ; break ;

	case 4: config |= CONFIG_MUX_DIFF_0_1 ; break ;
	case 5: config |= CONFIG_MUX_DIFF_2_3 ; break ;
	case 6: config |= CONFIG_MUX_DIFF_0_3 ; break ;
	case 7: config |= CONFIG_MUX_DIFF_1_3 ; break ;
	}

	//	Start a single conversion
	char temp[3];
	temp[0] = 1;
	temp[1] = (config >> 8);
	temp[2] = (config & 0xFF);
	
	bcm2835_i2c_setSlaveAddress(i2cAddr); 
	bcm2835_i2c_write(temp,3);

}

static int16_t i2c_read16(uint8_t reg)
{
	char temp[2];
	temp[0] = reg;
	bcm2835_i2c_write(temp,1);
	bcm2835_i2c_read(temp,2);
	return temp[1] | temp[0] << 8;
}

static bool adc_ready(void)
{
	bcm2835_i2c_setSlaveAddress(i2cAddr); 
	int16_t result = i2c_read16(1);

	if ((result & CONFIG_OS_MASK) != 0) return true;
	return false;
}

static void adc_finish(int chan,int * adc_data)
{
	bcm2835_i2c_setSlaveAddress(i2cAddr); 
	int16_t result = i2c_read16(0);
	
	//Sometimes with a 0v input on a single-ended channel the internal 0v reference
	//can be higher than the input, so you get a negative result...
	if (result < 0) result = 0;
	
	/* save data and move to next channel */
	adc_data[chan] = result;

}

/*
* analogWrite:
*	We're using this to write to the 2 comparitor threshold registers.
*	We could use a digitalWrite here but as it's an analog comparison
*	then it feels better to do it this way.
*********************************************************************************
*/
/*
void myAnalogWrite (int chan, int data)
{

	int reg ;
	int16_t ndata ;

	chan &= 3 ;

	reg = chan + 2 ;

	if (data < -32767)
	ndata = -32767 ;
	else if (data > 32767)
	ndata = 32767 ;
	else
	ndata = (int16_t)data ;

	ndata = __bswap_16 (ndata) ;
	wiringPiI2CWriteReg16 (node->fd, reg, data) ;
}*/


/*
* ads1115Setup:
*	Create a new wiringPi device node for an ads1115 on the Pi's
*	I2C interface.
*********************************************************************************
*/

void ads1115_setup(uint8_t address,int gain_idx,int dr_idx )
{
	i2cAddr = address;
	if (dr_idx > 7) dr_idx = 7;   // Use default if out of range
	if (gain_idx > 5) gain_idx = 2 ;  // Use default if out of range
		
	global_dr = dataRates[dr_idx];	
	global_gain = gains[gain_idx];
}


int * ads1115_update(void){

	static bool adc_busy = false;
	static int adc_active_channel = 0; //what channel we are working on
	
	static int adc_data[4];
	
	if (adc_busy) { 
		if (adc_ready()){  //superfulous
			adc_finish(adc_active_channel,adc_data);
			adc_busy = false;
			adc_active_channel++;
			if (adc_active_channel > 3) adc_active_channel = 0;
		} else {
			/* under normal i2c update cadence calling this should never happen */
			printf("Waiting on ADC....\n"); 
		}
	} else {
		adc_busy = true;
		adc_start(adc_active_channel);
	}
	return adc_data;
}