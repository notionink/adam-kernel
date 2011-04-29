/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
 */

#ifndef __ISL29023_H_INCLUDED__
#define __ISL29023_H_INCLUDED__

#define ISL29023_LS_DEVICE_NAME				"light_sensor"

#define ISL29023_I2C_ADDRESS				(0x88)	
#define ISL29023_UPDATE_INTERVAL			(300)

struct isl29023_platform_data {
	int i2c_instance;
	int i2c_address;
	int intr_gpio;
	int update_interval; 
};

#endif
