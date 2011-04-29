/*
 * File        : kernel/driver/power/yoku_0663113/sbi.c
 * Description : smart battery interface inplement.
 * Author      : LiuZheng <xmlz@malata.com> 2010/05/22
 */
#include <linux/kernel.h>
#include <linux/err.h> 

#include "odm_i2c.h"
#include "odm_sbi.h"
#include "logd.h"

int Temperature(struct odm_i2c_dev *dev, unsigned short *temp)
{
	if (odm_i2c_read_word(dev, SBI_TEMPERATURE, (unsigned short *)temp)) {
		logd("Temperature err.\r\n");
		return -1;
	}
	return 0;
}

int Voltage(struct odm_i2c_dev *dev, unsigned short *voltage)
{
	if (odm_i2c_read_word(dev, SBI_VOLTAGE, voltage)) {
		logd ("Voltage err.\r\n");
		return -1;
	}
	return 0;
}

int Current(struct odm_i2c_dev *dev, short *current)
{
	if (odm_i2c_read_word(dev, SBI_CURRENT, (unsigned short *)current)) {
		logd ("Current err.\r\n");
		return -EINVAL;
	}
	return 0;
}

int RelativeStateOfCharge(struct odm_i2c_dev *dev, unsigned short *rsoc)
{
	if (odm_i2c_read_word(dev, SBI_RELATIVE_STATE_OF_CHARGE, (unsigned short *)rsoc)) {
		logd("RelativeStateOfCharge err.\r\n");
		return -EINVAL;
	}
	return 0;
}

int RemainingCapacity(struct odm_i2c_dev *dev, unsigned short *capacity)
{
	return odm_i2c_read_word(dev, SBI_REMAINING_CAPACITY, (unsigned short *)capacity);
}

int FullChargeCapacity(struct odm_i2c_dev *dev, unsigned short *capacity)
{
	return odm_i2c_read_word(dev, SBI_FULL_CHARGE_CAPACITY, (unsigned short *)capacity);
}

int AverageTimeToEmpty(struct odm_i2c_dev *dev, unsigned short *time)
{
	if (odm_i2c_read_word(dev, SBI_AVERAGE_TIME_TO_EMPTY, (unsigned short *)time)) {
		logd("AverageTimeToEmpty err.\r\n");
		return -EINVAL;
	}
	return 0;
}

int BatteryStatus(struct odm_i2c_dev *dev, unsigned short *status)
{
	if (odm_i2c_read_word(dev, SBI_BATTERY_STATUS, status)) {
		logd("BatteryStatus err.\r\n");
		return -EINVAL;
	} 
	return 0;
}

int ChargingCurrent(struct odm_i2c_dev *dev, unsigned short *current)
{
	return odm_i2c_read_word(dev, SBI_CHARGING_CURRENT, current);
}

int ChargingVoltage(struct odm_i2c_dev *dev, unsigned short *voltage)
{
	return odm_i2c_read_word(dev, SBI_CHARGING_VOLTAGE, voltage);
}

int DesignCapacity(struct odm_i2c_dev *dev, unsigned short *capacity)
{
	return odm_i2c_read_word(dev, SBI_DESIGN_CAPACITY, capacity);
}

int DesignVoltage(struct odm_i2c_dev *dev, unsigned short *voltage)
{
	return odm_i2c_read_word(dev, SBI_DESIGN_VOLTAGE, voltage);
}

int ManufactureDate(struct odm_i2c_dev *dev, unsigned short *date)
{
	return odm_i2c_read_word(dev, SBI_MANUFACTURE_DATE, date);
}

int SerialNumber(struct odm_i2c_dev *dev, unsigned short *number)
{
	return odm_i2c_read_word(dev, SBI_SERIAL_NUMBER, number);
}

int ManufactureName(struct odm_i2c_dev *dev, unsigned char **name)
{
	return odm_i2c_block_read(dev, SBI_MANUFACTURE_NAME, name);
}

int DeviceName(struct odm_i2c_dev *dev, unsigned char **name)
{
	return odm_i2c_block_read(dev, SBI_DEVICE_NAME, name);
}

int DeviceChemistry(struct odm_i2c_dev *dev, unsigned char **chemistry)
{
	return odm_i2c_block_read(dev, SBI_DEVICE_CHEMISTRY, chemistry);
}

int AverageVoltage(struct odm_i2c_dev *dev, unsigned short *voltage) 
{	
	if (odm_i2c_read_word(dev, SBI_AVERAGE_VOLTAGE, voltage)) {
		logd("AverageVoltage err.\r\n");
		return -1;
	}
	return 0;
}

int AverageCurrent(struct odm_i2c_dev *dev, short *current)
{
	if (odm_i2c_read_word(dev, SBI_AVERAGE_CURRENT, current)) {
		logd("AverageCurrent err.\r\n");
		return -EINVAL;
	}
	return 0;
}

int RunTimeToEmpty(struct odm_i2c_dev *dev, unsigned short *minute)
{
	if (odm_i2c_read_word(dev, SBI_RUN_TIME_TO_EMPTY, minute)) {
		logd("RunTimeToEmpty err.\r\n");
		return -EINVAL;
	}
	return 0;
}

int StateOfHealth(struct odm_i2c_dev *dev, unsigned short *health)
{
	if (odm_i2c_read_word(dev, SBI_STATE_OF_HEALTH, health)) {
		logd("StateOfHealth err.\r\n");
		return -EINVAL;
	}
	return 0;
}

