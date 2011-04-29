/*
 *********************************************************************
 * File        : yoku_0563113/odm_sbi.h
 * Description : This file implement the Smart battery interface.
 * Note        : 
 * Author      : LiuZheng <xmlz@malata.com> 2010/05/21
 *********************************************************************
 */
#ifndef __ODM_SBI_H_INCLUDED__
#define __ODM_SBI_H_INCLUDED__
/**
 * BatteryMode.
 * 
 * Description : This function reports the battery system's operational modes and 
 *		capabilities, and flags minor conditions requiring attention.
 * Purpose : To allow the Host Device to determine the presence of Fule Cell system
 *		and the particular data reporting formats.
 * SMBus Protocol: Read or Write Word
 * TODO: what is this doing?
 */
#define SBI_BATTERY_MODE 				0x30


/** 
 * Temperature(0x08).
 * Description :This read-word function returns a unsigned integer value of the
 * 		temperature in units of 0.1K as measured by the bq20z70. It has a range 
 *		of 0~6553.5K.
 * SMBus protocol : read word
 * Out: unsigned 16-bit.
 */
#define SBI_TEMPERATURE					0x08

/**
 * Voltage (0x09)
 * Description : Value of the sum of individual of cell voltage measures in mV with 
 * 		a range of 0~20000mV.
 * SMBus Protocol : read word.
 * Out : unsigned 16-bit.
 */
#define SBI_VOLTAGE						0x09

/**
 * Current (0x0A).
 *
 * SMBus protocol : Read Word.
 * Out : signed 16-bit integar.
 */ 

#define SBI_CURRENT						0x0A

/**
 * RelativeStateOfCharge(0Dh).
 * 
 * Description : Returns the predicated remaining battery capacity or Fule Cell system fule
 * 		cartridge expressed as a precentage of the DesignCapacity(%)
 * Purpose : The RelativeStateOfCharge function is used to estimate the amount of charge
 * 		remaining in the battery or fule in the Fule Cell system fule cartridge(s).
 * SMBus Protocol : Read Word
 * Output : unsigned int - percent of remaining capacity
 *			Unitis: %
 *			Range: 0 to 100%
 *			Granularity: 2%
 *			Accuracy : 10%
 * 
 * BQ20Z70:  This value is expressed in either charge(mAh) or energy(10mWh), depending 
 *		on the setting of [CAPACITY_MODE] flag.
 */
#define SBI_RELATIVE_STATE_OF_CHARGE		0x0D

/**
 * RemaningCapacity (0Fh)
 * Description : Returns the predicated remaining battery capacity of Fule Cell
 *		system internal battery capacity in milli-Amp-hour(mAh)
 * SMBus protocol : Read Word
 * Output :	Units : mAh
 * 			Range : 0-65535mAh
 */
#define SBI_REMAINING_CAPACITY				0x0F

/**
 * FullChargeCapacity(10h)
 * 
 * Description : Returns the predicated pack capacity or Fuel Cell system internal  *		battery capacity when it is fully charged in milli-Amp-Hours(mAh)
 * SMBus Protocal : Read Word
 * Output : unsigned int - estimated full charge capacity in mAh.
 * 		Units: mAh
 *		Range: 0 ~ 65535
 * 
 * BQ20Z70: This value is expressed in either charge(mAh) or power(mWh) depending on 
 * 		the setting of [CAPACITY_MODE] flag.
 */
#define SBI_FULL_CHARGE_CAPACITY			0x10

/**
 * AverageTimeToEmpty(0x12h)
 * Description: Returns a rolling average of the predicated remaining battery life
 *		or Fule Cell system fule cartridge remaining runtime in minutes.
 * SMBus protocol : Read Word
 * Output : unsigned int - minute of operation left.
 *		Unit: minutes
 *
 * BQ20Z70: A range of 0~65534. A value of 65535 indicates that the battery is not
 * 		being discharged.
 *		0~05535 = predicted remaining battery life, based on AverageCurrent
 *		65535 = battery is not being discharged.
 */
#define SBI_AVERAGE_TIME_TO_EMPTY			0x12


/**
 * BatteryStatus (16h).
 * Description : Returns the status word which contains alarm and status bit flags
 * 		which indicate end-of-discharge, over temperature, and other conditions.
 * Purpose: The BatteryStatus() function is used by the Host Device to get alram
 *		and status bits, as well as error code from the Smart Battery.
 * SMBus Protocol : Read Word.
 * Out : unsigned 16-bit.
 *
 * --------------Alarm Bits--------------
 * 0x8000 OVER_CHARGED_ALARM
 * 0x4000 TERMINATE_CHARGE_ALARM
 * 0x2000 reserved
 * 0x1000 OVER_TEMP_ALARM
 * 0x0800 TERMINATE_DISCHARGE_ALARM
 * 0x0400 reserved
 * 0x0200 REMAINING_CAPACITY_ALARM
 * 0x0100 REMAINING_TIME_ALARM
 *
 * -------------Status Bits--------------
 * 0x0080 INITIALIZED
 * 0x0040 DISCHARGING
 * 0x0020 FULLY_CHARGED
 * 0x0010 FULLY_DISCHARGED
 */
#define SBI_BATTERY_STATUS 					0x16

#define SBI_BS_OVER_CHARGED_ALARM			0x8000
#define SBI_BS_TERMINATE_CHARGE_ALARM		0x4000
#define SBI_BS_OVER_TEMP_ALARM				0x1000
#define SBI_BS_TERMINATE_DISCHARGE_ALARM	0x0800
#define SBI_BS_REMAINING_CAPACITY_ALARM		0x0200
#define SBI_BS_REMAINING_TIME_ALARM			0x0100
#define SBI_BS_ALL_ALARM					(0x8000 | 0x4000 | 0x1000 | 0x0800 | 0x0200 | 0x0100)

#define SBI_BS_INITILIZED					0x0080
#define SBI_BS_DISCHARGING					0x0040
#define SBI_BS_FULLY_CHARGED				0x0020
#define SBI_BS_FULLY_DISCHARGED				0x0010

/**
 * ChargingCurrent (14h)
 * Description : Returns the Smart Battery's desired charging rate in milli-Amps(mA)
 * Purpose : The Charging Current function returns the maximum current that a Smart
 * 		Battery Charger may deliver to the Smart Battery. In combination with the
 *		ChargingVoltage these functions permit a Smart Battery Charger to dynamically
 *		adjust its charging profile(current/voltage) for optimal charge. The battery
 * 		can effectively trun off the Smart Battery Charger by returning a value of 0
 *		for this function. Smart Battery Chargers may be operated as a constant 
 * 		voltage source above their maximum regulated current by returning a
 *		ChargingCurrent value of 65535.
 * Note : The Smart Battery Charger is expected to respond in one of three ways:
 *		1. Supply the current requested.
 *		2. Supply its maximum current if the request is greater than its maxmum and less
 *		than 65535.
 *		3. Supply its maximum safe current if the request is 65535.
 * Protocol : Read Word
 * Output: unsigned int -- maximum charger output current in mA(0~65535)
 */
#define SBI_CHARGING_CURRENT				0x14
#define SBI_CC_CONSTATN_VOLTAGE_CHARGING	65535

/**
 * ChargingVoltage(15h)
 * Description : Returns the SmartBattery's desired charging voltage in milli-Volts(mV)
 *		This represents the maximum voltage which may be provided by the Smart Battery
 *		Charger during charging.
 * SMBus protocol : Read Word
 * Output : unsigned int - charger output voltage in mV
 */
#define SBI_CHARGING_VOLTAGE				0x15
#define SBI_CV_CONSTANT_CURRENT_CHARGING	65535

/**
 * DesignCapacity(18h).
 * Description : Returns the full capacity of a new battery pack or Fule Cell system
 * 		fule cartidge(s) in milli-Amp-hours(mAh).
 * SMBus Protocol : Read Word
 * Output : unsigned int - the battery or Fule Cell system fule cartridge capacity in mAh
 *		Units: mAh
 *
 * BQ20Z70: The DesignCapacity value is expressed in either current(mAh) or power, 
 *		depending on the setting of [CAPACITY_MODE] bit.
 */
#define SBI_DESIGN_CAPACITY					0x18

/**
 * DesignVoltage(19h).
 * Description : Returns the design voltage of a new battery pack or Fule Cell system in 
 *		milli-Volts(mV).
 * SMBus Protocol : Read Word
 */
#define SBI_DESIGN_VOLTAGE					0x19

/**
 * ManufactureDate(1Bh).
 * Description : This function returns the date the cell pack was manufactured in a packed
 *		integer. The date is packed in the following fashion: (Year-1980)*512 + Moth*32 +Day
 * SMBus Protocol : Read Word
 */
#define SBI_MANUFACTURE_DATE				0x1B

/**
 * SerialNumber(1Ch).
 * Description: This function is used to return a serial number. The number when combined 
 * 		with the ManufactureName, the DeviceName, and the ManufactureDate will uniquely 
 *		indentify the battery.
 * SMBus Protocol : Read Word
 * Output : unsigned int (0~65535)
 */
#define SBI_SERIAL_NUMBER					0x1C

/**
 * ManufactureName(20h).
 * Desciption : This function returns a character array containing the battery or Fule
 *		Cell manufacture's name. For example, "BestBatt" would identify the battery
 * 		or Fule Cell's manufacture as BestBatt.
 * SMBus Protocol : Read Block
 * Ouput : string - limited to 8 characters.
 */
#define SBI_MANUFACTURE_NAME				0x20

/**
 * DeviceName(21h)
 * Description : This function returns a character string that contains the batterh or 
 * 		Fuel Cell's name. For example, a DeviceName of "SmartB" would indicate that the
 *		battery is a modle SmartB.
 * SMBus Protocol : Read Block
 * Output : string -- limited to 8 characters. 
 */
#define SBI_DEVICE_NAME						0x21

/**
 * Device Chemistry(22h)
 * Description : This function returns a character string that contains the battery or 
 *		Fule Cell's chemistry. For example, if the DeviceChemistry function returns 
 *		"LS02", the battery pack would comtain primary lithium cells.
 * SMBus protocol : Read Block
 * Ouput string - limited to 4 characters
 */
#define SBI_DEVICE_CHEMISTRY				0x22


/**
 * AverageVoltage(0x5d).
 * Description: Returns a signed integer value that appromixates a one-munite rolling 
 * 		average of the sume of cell voltages in mV, with a range of 0~65535.
 * SMBus Protocol: Read Word
 */
#define SBI_AVERAGE_VOLTAGE					0x5d

/**
 * AverageCurrent(0x0b).
 * Description: This function returns a signed integer value approximates a one-minute 
 * 		average of the current being suppied (or accepted) thorogh the battery 
 *		terminals in mA, with a range of -32768~32767.
 * SMBus protocol: Read Word.
 */
#define SBI_AVERAGE_CURRENT					0x0b

/**
 * RunTimeToEmpty(0x11)
 * Description: This function returns a unsigned integer value of oredicted remaining 
 *		battery life at the present rate of discharge, in minutes, with a range of
 *		0~65535min. A value of 65535 indicates battery is not being discharged.
 * SMBus protocol: Read Word.
 * Out: 16-bit unsigned short int.
 */
#define SBI_RUN_TIME_TO_EMPTY				0x11

/**
 * StateOfHealth(0x4f)
 * Description : This read word function returns the state of health of the battery in
 *		%. The caculation formula depends on the CAPACITY_MODE flag.
 * 		CAPACITY_MO StateOfHeath
 *		DE
 *		0 = FullChargeCapacity/Design Capacity
 *		1 = FullChargeCapacity/Design Energy
 * Return value : 16 bit unsigned integer, range : 0~100.
 */
#define SBI_STATE_OF_HEALTH					0x4f


/**
 * DataFlashSubClassID(0x77).
 * Description : This write word function set the bq27z20 dataflash subclass, where 
 *		data can be accessed by following DataFlashSubClass1..8 commands. A NAK is
 *		returned to this command if the value of the class is outside of the allowed
 *		range.
 * SMBus protocol: Write-word.
 */
#define SBI_DATA_FLASH_SUBCLASS_ID			0x77

/**
 * DataFlashSubClassPage1..8(0x78...0x7f)
 * Description: These comamnds are used to access the consecutive 32-byte pages of 
 * 		of each subclass. DataFlashSubClassPage1 get byte 0 to 31 of the subclass, 
 * 		DataFlashSubClassPage2 get bytes 32 to 63 and so on.
 */
#define SBI_DATA_FLASH_SUBCLASS_PAGE1		0x78
#define SBI_DATA_FLASH_SUBCLASS_PAGE2		0x79
#define SBI_DATA_FLASH_SUBCLASS_PAGE3		0x7a
#define SBI_DATA_FLASH_SUBCLASS_PAGE4		0x7b
#define SBI_DATA_FLASH_SUBCLASS_PAGE5		0x7c
#define SBI_DATA_FLASH_SUBCLASS_PAGE6		0x7d
#define SBI_DATA_FLASH_SUBCLASS_PAGE7		0x7e
#define SBI_DATA_FLASH_SUBCLASS_PAGE8		0x7f

#ifdef __BQ207Z0_USED__
#include "bq20z70.h"
#endif

/*
 * Method declare
 */
int Temperature(struct odm_i2c_dev *dev, unsigned short *temp);
int Voltage(struct odm_i2c_dev *dev, unsigned short *voltage);
int Current(struct odm_i2c_dev *dev, short *current);
int RelativeStateOfCharge(struct odm_i2c_dev *dev, unsigned short *rsoc);
int RemainingCapacity(struct odm_i2c_dev *dev, unsigned short *capacity);
int FullChargeCapacity(struct odm_i2c_dev *dev, unsigned short *capacity);
int AverageTimeToEmpty(struct odm_i2c_dev *dev, unsigned short *time);
int BatteryStatus(struct odm_i2c_dev *dev, unsigned short *status);
int ChargingCurrent(struct odm_i2c_dev *dev, unsigned short *current);
int ChargingVoltage(struct odm_i2c_dev *dev, unsigned short *voltage);
int DesignCapacity(struct odm_i2c_dev *dev, unsigned short *capacity);
int DesignVoltage(struct odm_i2c_dev *dev, unsigned short *voltage);
int ManufactureDate(struct odm_i2c_dev *dev, unsigned short *date);
int SerialNumber(struct odm_i2c_dev *dev, unsigned short *number);
int ManufactureName(struct odm_i2c_dev *dev, unsigned char **name);
int DeviceName(struct odm_i2c_dev *dev, unsigned char **name);
int DeviceChemistry(struct odm_i2c_dev *dev, unsigned char **chemistry);
int AverageVoltage(struct odm_i2c_dev *dev, unsigned short *voltage);
int AverageCurrent(struct odm_i2c_dev *dev, short *current);
int RunTimeToEmpty(struct odm_i2c_dev *dev, unsigned short *minute);
int StateOfHealth(struct odm_i2c_dev *dev, unsigned short *health);

#endif
