/*
 * SMBus reserved slave address
 * 0001100 : SMBus Alert Response Address.
 * 1100001 : SMBus Device Default Address.
 * 	The SMBus Device Default Address is reserved for use by the SMBus Address
 *	Resolution Protocol, which allows address to be assigned dynamically.
 *
 * SMBus Bus Protocols:
 * Quick command:	[S] [Slave Address] [R/W] [A] [P]
 * Send byte: [S] [Slave Address] [W] [A] [Data Byte] [A] [P]
 */
#include <linux/mm.h>
#include <linux/errno.h>
#include "nvcommon.h"
#include "nvos.h"
#include "nvodm_services.h"
#include "nvodm_query.h"

#include "odm_i2c.h"

#include "logd.h"

#define I2C_CLOCK_KHZ	100

void odm_i2c_init(struct odm_i2c_dev *dev, unsigned char instance, unsigned char address, int speed, int timeout)
{
	dev->i2c = 0;
	dev->instance = instance;
	dev->address = address;
	dev->speed = speed;
	dev->timeout = timeout;
}

void odm_smbus_init(struct odm_i2c_dev* dev, unsigned char instance,  unsigned char address, int timeout)
{
	odm_i2c_init(dev, instance, address, I2C_CLOCK_KHZ, timeout);
}

int odm_i2c_open(struct odm_i2c_dev *dev)
{
	dev->i2c = NvOdmI2cOpen(NvOdmIoModule_I2c, dev->instance);
	return (dev->i2c != 0) ? 0 : -EINVAL;
}

int odm_i2c_close(struct odm_i2c_dev *dev) 
{
	if (dev->i2c) {
		NvOdmI2cClose(dev->i2c);
	}
}

/* Send byte protocol*/
int odm_i2c_send_byte(struct odm_i2c_dev *dev, unsigned char data)
{
	NvOdmI2cTransactionInfo TransactionInfo;
	NvOdmI2cStatus I2cStatus;

	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = dev->address;
	TransactionInfo.Buf = &data;
	TransactionInfo.NumBytes = 1;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo, 
			1, dev->speed, dev->timeout);

	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_receive_byte(struct odm_i2c_dev *dev,  unsigned char *data)
{
	NvOdmI2cTransactionInfo TransactionInfo;
	NvOdmI2cStatus I2cStatus;

	TransactionInfo.Flags = 0;
	TransactionInfo.Address = dev->address;
	TransactionInfo.Buf = data;
	TransactionInfo.NumBytes = 1;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo, 
			1, dev->speed, dev->timeout);
	
	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_write_byte(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char data)
{
	unsigned char buffer[2];
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo;

	buffer[0] = cmd;
	buffer[1] = data;

	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = dev->address;
	TransactionInfo.Buf = buffer;
	TransactionInfo.NumBytes = 2;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo, 
			1, dev->speed, dev->timeout);

	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_read_byte(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char *data)
{
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo[2];

	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->address;
	TransactionInfo[0].Buf = &cmd;
	TransactionInfo[0].NumBytes = 1;
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->address | 0x01;
	TransactionInfo[1].Buf = data;
	TransactionInfo[1].NumBytes = 1;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo, 
			2, dev->speed, dev->timeout);
	
	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_write_word(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int data)
{
	unsigned char buffer[3];
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo;

	buffer[0] = cmd;
	buffer[1] = data | 0x00ff;
	buffer[2] = data >> 8;

	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = dev->address;
	TransactionInfo.Buf = buffer;
	TransactionInfo.NumBytes = 3;
	
	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo,
			1, dev->speed, dev->timeout);

	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_read_word(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int *data)
{
	unsigned char buffer[2];
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo[2];

	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->address;
	TransactionInfo[0].Buf = &cmd;
	TransactionInfo[0].NumBytes = 1;
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->address | 0x1;
	TransactionInfo[1].Buf = buffer;
	TransactionInfo[1].NumBytes = 2;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo,
			2, dev->speed, dev->timeout);

	if (I2cStatus != NvOdmI2cStatus_Success) {
		logd("I2cAddress=%d, I2cStatus=%d\r\n", dev->address, I2cStatus);
		return -EINVAL;
	}
	
	*data = buffer[0] | buffer[1] << 8;
	
	return 0;
}

int odm_i2c_process_call(struct odm_i2c_dev *dev, unsigned char cmd, unsigned short int write_data, unsigned short int *read_data)
{
	unsigned char write_buffer[3];
	unsigned char read_buffer[2];
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo[2];

	
	write_buffer[0] = cmd;
	write_buffer[1] = write_data & 0x00ff;
	write_buffer[2] = write_data >> 8;
	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->address;
	TransactionInfo[0].Buf = write_buffer;
	TransactionInfo[0].NumBytes = 3;
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->address | 0x1;
	TransactionInfo[1].Buf = read_buffer;
	TransactionInfo[1].NumBytes = 2;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo,
			2, dev->speed, dev->timeout);

	if (I2cStatus != NvOdmI2cStatus_Success) {
		return -1;
	}
	
	*read_data = read_buffer[0] | (read_buffer[1] << 8);
	return 0;
}

int odm_i2c_block_write(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char *buffer, unsigned char buffer_len)
{
	unsigned int i;
	unsigned char *write_buffer;
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo;

	write_buffer = kzalloc(buffer_len + 2, GFP_KERNEL);
	if (!write_buffer) {
		return -ENOMEM;
	}

	write_buffer[0] = cmd;
	write_buffer[1] = buffer_len;
	memcpy(write_buffer + 2, buffer, buffer_len);
	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = dev->address;
	TransactionInfo.Buf = write_buffer;
	TransactionInfo.NumBytes = buffer_len+2;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo,
			2, dev->speed, dev->timeout);
	
	kfree(write_buffer);
	return (I2cStatus == NvOdmI2cStatus_Success) ? 0 : -1;
}

int odm_i2c_block_read(struct odm_i2c_dev *dev, unsigned char cmd, unsigned char **buffer)
{
	/* TODO: How to implement this ? */
	/* OK, let's read twice, first time we get the total number, then get all the block data */
	int i;
	unsigned char data_len = 0;
	unsigned char *read_buffer;

	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo[2];

	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->address;
	TransactionInfo[0].Buf = &cmd;
	TransactionInfo[0].NumBytes = 1; 
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->address | 0x1;
	TransactionInfo[1].Buf = &data_len;
	TransactionInfo[1].NumBytes = 1;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo,
			2, dev->speed, dev->timeout);

	if (I2cStatus != NvOdmI2cStatus_Success || data_len <= 0) {
		return -EINVAL;
	}
	
	read_buffer = kzalloc(data_len+2, GFP_KERNEL); 
	if (!read_buffer) {
		return -ENOMEM;
	}
	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->address;
	TransactionInfo[0].Buf = &cmd;
	TransactionInfo[0].NumBytes = 1; 
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->address | 0x1;
	TransactionInfo[1].Buf = read_buffer;
	TransactionInfo[1].NumBytes = data_len + 1;

	I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo,
			2, dev->speed, dev->timeout);

	if (I2cStatus != NvOdmI2cStatus_Success) {
		kfree(read_buffer);
		return -EINVAL;
	}
	
	for (i = 0; i < data_len; i++) {
		read_buffer[i] = read_buffer[i+1];
	}
	read_buffer[i] = '\0';

	return 0;
}
