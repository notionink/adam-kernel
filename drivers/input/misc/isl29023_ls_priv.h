/*
 * File        : kernel/driver/input/misc
 * Description : Driver for isl29023 ambient light sensor driver for android system.
 * Athor       : LiuZheng <xmlz@malata.com>
 * Date        : 2010/06/03
 */

#ifndef __ISL29023_LS_PRIV_H_INCLUDED__
#define __ISL29023_LS_PRIV_H_INCLUDED__

/*
 * I2c parameter
 */
#define ISL29023_I2C_INSTANCE				0
#define ISL29023_I2C_SPEED_KHZ				(100)
#define ISL29023_I2C_TIMEOUT_MS				(1000)


/*
 * Register
 */
#define ISL29023_REG_COMMAND_I				(0x00)
#define ISL29023_REG_COMMAND_II				(0x01)
#define ISL29023_REG_DATA_LSB				(0x02)
#define ISL29023_REG_DATA_MSB				(0x03)
#define ISL29023_REG_INT_LT_LSB				(0x04)
#define ISL29023_REG_INT_LT_MSB				(0x05)
#define ISL29023_REG_INT_HT_LSB				(0x06)
#define ISL29023_REG_INT_HT_MSB				(0x07)

/*
 * ISL29023_REG_COMMAND_I command
 */
#define ISL29023_POWER_DOWN					(0x00)
#define ISL29023_ALS_CONTINUE				(0xA0)

/*
 * ISL29023_REG_COMMAND_II command
 * with a Rext = 499KO
 *
 * B3-B2 : Resolution 	
 * 	00 = 2^16, 01 = 2^12
 *	10 = 2^8,  11 = 2^4
 * B1-B0 : Range 
 * 	00 = 1000,  01 = 4000 
 * 	10 = 16000, 11 = 64000
 * 
 * ISL29023_MODE_RANGE: 		resolution=2^16 range=64000
 * ISL29023_MODE_RESOLUTION:    resolution=2^16 range=1000
 *
 */
#define ISL29023_MODE_RANGE					(0x03)
#define ISL29023_MODE_RESOLUTION			(0)

#define ISL29023_R2M_RANGE					(64000*1000/65536)
#define ISL29023_R2M_RESOLUTION				(1000*1000/65536)
#endif

/*
 * ACCURACY TYPE
 */
#define ISL29023_ACCURACY_HIGH				1
#define ISL29023_ACCURACY_MEDIUM			2
#define ISL29023_ACCURACY_LOW				3

#define ACCURACY_LEVEL						7

#define ISL29023_DEFAULT_ACCURACY			ISL29023_ACCURACY_MEDIUM
