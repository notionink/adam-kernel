/*
 *********************************************************************
 * File name   : 
 * Description : 
 * Log         : 
 *********************************************************************
 */
#ifndef INCLUDE_NVODM_TOUCH_AK4183_INT_H
#define INCLUDE_NVODM_TOUCH_AK4183_INT_H

#define AK4183_TOUCH_DEVICE_GUID 			NV_ODM_GUID('a','k','4','1','8','3','t','s')
#define AK4183_I2C_SPEED_KHZ				(400)
#define AK4183_I2C_TIMEOUT					(10)
#define AK4183_DEBOUNCE_TIME_MS				(0)

/* AK4183 command */
#define AK4183_CMD_ACTIVE_X					(0x84)
#define AK4183_CMD_ACTIVE_Y					(0x94)
#define AK4183_CMD_ACCELERATE_X				(0x84)
#define AK4183_CMD_ACCELERATE_Y				(0x94)
#define AK4183_CMD_MEASURE_X				(0xC0)
#define AK4183_CMD_MEASURE_Y				(0xD0)
#define AK4183_CMD_MEASURE_Z1				(0xE0)
#define AK4183_CMD_MEASURE_Z2				(0xF0)
#define AK4183_CMD_SLEEP_ON					(0x80)

#define AK4183_CMD_INIT						(0x80)

#ifndef NVODM_I2C_7BIT_ADDRESS
#define NVODM_I2C_7BIT_ADDRESS(Addr7Bit)	((Addr7Bit) << 1)
#endif

//#define __AK4183_DEBUG

#ifdef __AK4183_DEBUG
#define AK4183_PRINTF(x)	NvOsDebugPrintf x;
#else
#define AK4183_PRINTF(x)
#endif

#ifndef __AK4183_DEBUG
// #define __AK4183_TEST
#endif

#if 0
//for a8901 touch screen
#define AK4183_MAX_X		(4024)
#define AK4183_MAX_Y		(3900)
#define AK4183_MIN_X		(300)
#define AK4183_MIN_Y		(220)
#else
//for new touch screen
#define AK4183_MAX_X		(3960)
#define AK4183_MAX_Y		(3800)
#define AK4183_MIN_X		(100)
#define AK4183_MIN_Y		(160)
#endif

typedef struct AK4183_TouchDeviceRec {
	NvOdmTouchDevice 		OdmTouch;
	NvOdmTouchCapabilities	Caps;
	NvOdmServicesI2cHandle	hOdmI2c;
	NvU32					DeviceAddress;
	NvU32					I2cClockSpeedKHz;
	NvOdmServicesGpioHandle	hGpio;
	NvOdmGpioPinHandle		hPin;
	NvOdmServicesGpioIntrHandle hGpioIntr;
	NvOdmOsSemaphoreHandle	hIntrSema;
	NvU32 					SleepMode;
} AK4183_TouchDevice;


#endif // INCLUDE_NVODM_TOUCH_AK418_INT_H
