/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
 */
#ifndef __LIS35DE_ACCEL_H_INCLUDED__
#define __LIS35DE_ACCEL_H_INCLUDED__

/*
 * I2c address: 
 * SDO = 0: read/write = 0x39/0x38
 * SDO = 1: read/write = 0x3b/0x3a
 */
#define LIS35DE_I2C_ADDRESS		0x38

/*
 * flag
 */
#define LIS35DE_FLIP_X			0x01
#define LIS35DE_FLIP_Y			0x02
#define LIS35DE_FLIP_Z			0x04
#define LIS35DE_SWAP_XY			0x08

#define LIS35DE_DEVICE_NAME		"accelerometer"

struct lis35de_platform_data {
	int i2c_instance;
	int i2c_address;
	int update_interval;
	int intr_gpio;
	int flag;
};

#endif
