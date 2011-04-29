/*
 * File : __i2c__.h
 * Description : 
 * Note : 
 * Author : LiuZheng <xmlz@malata.com> 2010/05/21
 */
#ifndef __ODM_I2C_H_INCLUDED__
#define __ODM_I2C_H_INCLUDED__

#include "nvodm_services.h"

struct odm_i2c_dev {
	NvOdmServicesI2cHandle i2c;
	unsigned char   instance;
	unsigned char 	address;
	unsigned int 	speed;
	unsigned int	timeout;
};

void odm_i2c_init(struct odm_i2c_dev *dev, unsigned char instance, unsigned char address, int speed, int timeout);

void odm_smbus_init(struct odm_i2c_dev* dev, unsigned char instance,  unsigned char address, int timeout);

int odm_i2c_open(struct odm_i2c_dev *dev);

int odm_i2c_close(struct odm_i2c_dev *dev);

int odm_i2c_send_byte(struct odm_i2c_dev *dev, unsigned char data);

int odm_i2c_receive_byte(struct odm_i2c_dev *dev,  unsigned char *data);

int odm_i2c_write_byte(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char data);

int odm_i2c_read_byte(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char *data);

int odm_i2c_write_word(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int data);

int odm_i2c_read_word(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int *data);

int odm_i2c_process_call(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int write_data, unsigned short int *read_data);

int odm_i2c_block_write(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char *buffer, unsigned char buffer_len);

int odm_i2c_block_read(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char **buffer);
#endif // __ODM_I2C_H_INCLUDED__
