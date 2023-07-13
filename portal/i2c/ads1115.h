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

#ifndef _ADS1115_H
#define _ADS1115_H


// Constants for some of the internal functions

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


int * ads1115_update(void);
void ads1115_setup(char * handle_name, uint8_t address,int gain_idx,int dr_idx);

#endif
