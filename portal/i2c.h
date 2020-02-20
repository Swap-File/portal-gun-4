#ifndef _I2C_H
#define _I2C_H

#include "common/memory.h"

void i2c_update(struct gun_struct *this_gun);
void i2c_init(void);

//	Gyro Defines

#define L3G_WHO_AM_I      0x0F

#define L3G_CTRL_REG1     0x20
#define L3G_CTRL_REG2     0x21
#define L3G_CTRL_REG3     0x22
#define L3G_CTRL_REG4     0x23
#define L3G_CTRL_REG5     0x24
#define L3G_REFERENCE     0x25
#define L3G_OUT_TEMP      0x26
#define L3G_STATUS_REG    0x27

#define L3G_OUT_X_L       0x28
#define L3G_OUT_X_H       0x29
#define L3G_OUT_Y_L       0x2A
#define L3G_OUT_Y_H       0x2B
#define L3G_OUT_Z_L       0x2C
#define L3G_OUT_Z_H       0x2D

#define L3G_FIFO_CTRL_REG 0x2E
#define L3G_FIFO_SRC_REG  0x2F

#define L3G_INT1_CFG      0x30
#define L3G_INT1_SRC      0x31
#define L3G_INT1_THS_XH   0x32
#define L3G_INT1_THS_XL   0x33
#define L3G_INT1_THS_YH   0x34
#define L3G_INT1_THS_YL   0x35
#define L3G_INT1_THS_ZH   0x36
#define L3G_INT1_THS_ZL   0x37
#define L3G_INT1_DURATION 0x38

//	ADC Defines

//	Gain

#define	ADS1115_GAIN_6		0
#define	ADS1115_GAIN_4		1
#define	ADS1115_GAIN_2		2
#define	ADS1115_GAIN_1		3
#define	ADS1115_GAIN_HALF	4
#define	ADS1115_GAIN_QUARTER	5

//	Data rate

#define	ADS1115_DR_8		0
#define	ADS1115_DR_16		1
#define	ADS1115_DR_32		2
#define	ADS1115_DR_64		3
#define	ADS1115_DR_128		4
#define	ADS1115_DR_250		5
#define	ADS1115_DR_475		6
#define	ADS1115_DR_860		7

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

#endif