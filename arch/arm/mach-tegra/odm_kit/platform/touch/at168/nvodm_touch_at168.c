/*
 * Copyright (c) 2007-2009 NVIDIA Corporation.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the NVIDIA Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "nvodm_services.h"
#include "nvodm_touch_at168.h"
#include "nvodm_query_discovery.h"
#include "nvodm_touch_at168_int.h"


#include "nvos.h"
#include "nvodm_query.h"

#include "../nvodm_touch_int.h"
#include "nvodm_touch.h"

#include "nvodm_query_gpio.h"

#include <linux/delay.h>

#include <linux/fs.h> 
#include <asm/uaccess.h> 
#include <linux/mm.h> 
#define AT168_I2C_SPEED_KHZ                          100//400
#define AT168_I2C_TIMEOUT                            2000//500
#define AT168_DEBOUNCE_TIME_MS 		0
#define AT168_TOUCH_DEVICE_GUID 			NV_ODM_GUID('a','t','e','1','6','8','t','s')

#define AT168_WRITE(dev, reg, byte) AT168_WriteRegister(dev, reg, byte)
#define AT168_READ(dev, reg, buffer, len) AT168_ReadRegisterSafe(dev, reg, buffer, len)

typedef struct AT168_TouchDeviceRec
{
	NvOdmTouchDevice OdmTouch;
	NvOdmTouchCapabilities Caps;
	NvOdmServicesI2cHandle hOdmI2c;
	NvOdmServicesGpioHandle hGpio;
	NvOdmServicesPmuHandle hPmu;
	NvOdmGpioPinHandle hPinReset;
	NvOdmGpioPinHandle hPinInterrupt;
#if defined(CONFIG_7373C_V20)
	NvOdmGpioPinHandle hPinPower;
#endif
	NvOdmServicesGpioIntrHandle hGpioIntr;
	NvOdmOsSemaphoreHandle hIntSema;
	NvBool PrevFingers;
	NvU32 DeviceAddr;
	NvU32 SampleRate;
	NvU32 SleepMode;
	NvBool PowerOn;
	NvU32 VddId;    
	NvU32 ChipRevisionId; //Id=0x01:AT168 chip on Concorde1
	                  //id=0x02:AT168 chip with updated firmware on Concorde2
	NvU32 I2cClockSpeedKHz;
} AT168_TouchDevice;

#if 0
//MAX and MIN of coord (x y)
#define AT168_MAX_X		(1024) //(4992)//(4096)//(1024)
#define AT168_MAX_Y		(600) //(2816)//(4096)//(600)
#define AT168_MIN_X		(0)
#define AT168_MIN_Y		(0)
#endif

static NvOdmTouchCapabilities AT168_Capabilities =
{
	.IsMultiTouchSupported = NV_TRUE,
	.MaxNumberOfFingerCoordReported = 2,
	.IsRelativeDataSupported = NV_FALSE,
	.MaxNumberOfRelativeCoordReported = 1,
	.MaxNumberOfWidthReported =1,
	.MaxNumberOfPressureReported = 1,
	.Gesture = NvOdmTouchGesture_Not_Supported,
	.IsWidthSupported = NV_TRUE,
	.IsPressureSupported = NV_TRUE,
	.IsFingersSupported = NV_TRUE,
	.XMinPosition = 0,//AT168_MIN_X,
	.YMinPosition = 0,//AT168_MIN_Y,
	.XMaxPosition = 1024,//AT168_MAX_X,
	.YMaxPosition = 600,//AT168_MAX_Y,
	.Orientation = 0, //0,//NvOdmTouchOrientation_V_FLIP,//NvOdmTouchOrientation_H_FLIP
	.Version = 0, 
	.CalibrationData = 0, 
	.BaselineDate = {0}, 
	.CalibrateResultDate = {0},
};

static NvBool AT168_Bootloader_Read (AT168_TouchDevice *hTouch, NvU8* buffer, NvU32 NumBytes)
{
	NvOdmI2cStatus Error;
	NvOdmI2cTransactionInfo TransactionInfo;
	NvU32 TempI2cClockSpeedKHz;

	//hTouch->DeviceAddr = (NvU32)((0x5d << 1) | 0x01);	//change I2C  to bootloader address 0x5d
	TempI2cClockSpeedKHz = hTouch->I2cClockSpeedKHz;
	hTouch->I2cClockSpeedKHz = 100;		//change I2C  freq to 100KHz

	TransactionInfo.Address = hTouch->DeviceAddr;
	TransactionInfo.Buf = buffer;
	TransactionInfo.Flags = 0;
	TransactionInfo.NumBytes = NumBytes;
    
	do
	{
		Error = NvOdmI2cTransaction(hTouch->hOdmI2c,
		                            &TransactionInfo,
		                            1,
		                            hTouch->I2cClockSpeedKHz,
		                            AT168_I2C_TIMEOUT);
	} while (Error == NvOdmI2cStatus_Timeout);

	if (Error != NvOdmI2cStatus_Success)
	{
		NvOsDebugPrintf("I2C Read Failure = %d  addr=0x%x \n", Error,
		                   hTouch->DeviceAddr);
		
		hTouch->I2cClockSpeedKHz = TempI2cClockSpeedKHz;		//change I2C  freq back to Normal
		//hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
		return NV_FALSE;
	}
	hTouch->I2cClockSpeedKHz = TempI2cClockSpeedKHz;		//change I2C  freq back to Normal
	//hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
	return NV_TRUE;
}
static NvBool AT168_Bootloader_Write(AT168_TouchDevice* hTouch, NvU8 *buffer, NvU8 NumBytes)
{
	NvOdmI2cTransactionInfo TransactionInfo;
	NvOdmI2cStatus err;
	NvU32 TempI2cClockSpeedKHz;

	//hTouch->DeviceAddr = (NvU32)(0x5d << 1);	//change I2C  to bootloader address 0x5d
	TempI2cClockSpeedKHz = hTouch->I2cClockSpeedKHz;
	hTouch->I2cClockSpeedKHz = 100;		//change I2C  freq to 100KHz

	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = hTouch->DeviceAddr;
	TransactionInfo.NumBytes = NumBytes;
	TransactionInfo.Buf = buffer;
	err = NvOdmI2cTransaction(hTouch->hOdmI2c,
								&TransactionInfo,
								1,
								hTouch->I2cClockSpeedKHz,
								AT168_I2C_TIMEOUT);
	if (err != NvOdmI2cStatus_Success) {
		NvOsDebugPrintf("NvOdmTouch: AT168_Bootloader_Write i2c transaction failture = %d, address=0x%x\n", err, hTouch->DeviceAddr);
		hTouch->I2cClockSpeedKHz = TempI2cClockSpeedKHz;		//change I2C  freq back to Normal
		//hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
		return NV_FALSE;
	}
	hTouch->I2cClockSpeedKHz = TempI2cClockSpeedKHz;		//change I2C  freq back to Normal
	//hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
	return NV_TRUE;
}

static NvBool AT168_WriteRegister (AT168_TouchDevice* hTouch, NvU8 reg, NvU8 val)
{
	NvOdmI2cStatus Error;
	NvOdmI2cTransactionInfo TransactionInfo;
	NvU8 arr[2];

	arr[0] = reg;
	arr[1] = val;

	TransactionInfo.Address = hTouch->DeviceAddr;
	TransactionInfo.Buf = arr;
	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.NumBytes = 2;

	do
	{
		Error = NvOdmI2cTransaction(hTouch->hOdmI2c,
		                            &TransactionInfo,
		                            1,
		                            hTouch->I2cClockSpeedKHz,
		                            AT168_I2C_TIMEOUT);
	} while (Error == NvOdmI2cStatus_Timeout); 

	if (Error != NvOdmI2cStatus_Success)
	{
		NvOsDebugPrintf("I2C Write Failure = %d (addr=0x%x, reg=0x%x, val=0x%0x)\n", Error, 
		                   hTouch->DeviceAddr, reg, val);
		return NV_FALSE;
	}
	
	return NV_TRUE;
}

static NvBool AT168_ReadRegisterOnce (AT168_TouchDevice* hTouch, NvU8 reg, NvU8* buffer, NvU32 len)
{
	NvOdmI2cStatus Error;
	NvOdmI2cTransactionInfo TransactionInfo[2 ];


	TransactionInfo[0].Address = hTouch->DeviceAddr;
	TransactionInfo[0].Buf = &reg;
	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].NumBytes = 1;

	TransactionInfo[1].Address = hTouch->DeviceAddr | 0x1;
	TransactionInfo[1].Buf = buffer;
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].NumBytes = len;
    
	do
	{
		Error = NvOdmI2cTransaction(hTouch->hOdmI2c,
		                            TransactionInfo,
		                            2,
		                            hTouch->I2cClockSpeedKHz,
		                            AT168_I2C_TIMEOUT);
	} while (Error == NvOdmI2cStatus_Timeout);

	if (Error != NvOdmI2cStatus_Success)
	{
		NvOsDebugPrintf("I2C Read Failure = %d (addr=0x%x, reg=0x%x)\n", Error,
		                   hTouch->DeviceAddr, reg);
		return NV_FALSE;
	}

	return NV_TRUE;
}

static NvBool AT168_ReadRegisterSafe (AT168_TouchDevice* hTouch, NvU8 reg, NvU8* buffer, NvU32 len)
{
    
	if (!AT168_ReadRegisterOnce(hTouch, reg, buffer, len))
		return NV_FALSE;

	return NV_TRUE;
}

static void InitOdmTouch (NvOdmTouchDevice* Dev)
{
	Dev->Close              = AT168_Close;
	Dev->GetCapabilities    = AT168_GetCapabilities;
	Dev->ReadCoordinate     = AT168_ReadCoordinate;
	Dev->EnableInterrupt    = AT168_EnableInterrupt;
	Dev->HandleInterrupt    = AT168_HandleInterrupt;
	Dev->GetSampleRate      = NULL;
	Dev->SetSampleRate      = NULL;
	Dev->PowerControl       = AT168_PowerControl;
	Dev->PowerOnOff         = AT168_PowerOnOff;
	Dev->GetCalibrationData = NULL;
	Dev->SetCalibration = AT168_SetCalibration;
	Dev->BurnBootloader = AT168_BurnBootloader;
	Dev->SetBaseline = AT168_SetBaseline;
	Dev->SetCalibrateResult = AT168_SetCalibrateResult;
	Dev->OutputDebugMessage = NV_FALSE;
}

void AT168_SetCalibration(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	NvU8 k = 0;
	//set the SPECOP reg and the touchscreen will calibration by itself;
RetrySetCalibration:
	if(!(AT168_WRITE(hTouch, AT168_SPECOP, AT168_SPECOP_CALIBRATION_VALUE)))
	{
			if(k < 60){
				k++;
				NvOsDebugPrintf("AT168_SetCalibration fail num is (%d) \n", k);
				msleep(5);
				goto RetrySetCalibration;
			}
			else{
				NvOsDebugPrintf("AT168_SetCalibration fail \n");
				AT168_Capabilities.CalibrationData = 2 ;
			}
	}
	else{
		NvOsDebugPrintf("AT168_SetCalibration OK \n");
		AT168_Capabilities.CalibrationData = 1 ;
	}

	//Copy AT168_Capabilities data to hTouch->Caps
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
}

void AT168_SetBaselineSintekTango(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	NvOsDebugPrintf("AT168_SetBaselineSintek is Sintek Tango IC \n");

	int i = 0;
	int j= 0;
	NvBool isodd = NV_TRUE;

	if(!(AT168_WRITE(hTouch, AT168_INTERNAL_ENABLE, AT168_INTERNAL_DISABLE_VALUE)))
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek  AT168_INTERNAL_DISABLE_VALUE fail \n");
	}
	else
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek  AT168_INTERNAL_DISABLE_VALUE OK \n");
	}

	msleep(100);

	//Sintek two Tango IC have 37 X line(30 in IC 1 / 7 in IC 2) and 21 Y line;
	NvU8 BaselineXValue1Temp[60] = {0};
	NvU8 BaselineXValue2Temp[14] = {0};
	NvU8 BaselineYValueTemp[42] = {0};

	NvS16 BaselineXValue1[30] = {0};
	NvS16 BaselineXValue2[7] = {0};
	NvS16 BaselineYValue[21] = {0};

	//Read X1 baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_BASELINE_X1_VALUE, BaselineXValue1Temp, 60))
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek: AT168_SINTEK_BASELINE_X1_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek:  AT168_SINTEK_BASELINE_X1_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_X1_VALUE temp [%d] = 0x%x---\n", i, BaselineXValue1Temp[i]);
			i++;

		}while(i < 60);
		#endif

		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				BaselineXValue1[j] =  BaselineXValue1Temp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((BaselineXValue1Temp[i]) & 0x80) 
				{
					BaselineXValue1[j] = ((BaselineXValue1Temp[i] << 8) |BaselineXValue1[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X1_VALUE temp 1 [%d] = %d---\n", j, BaselineXValue1[j]);
				}
				else
				{
					BaselineXValue1[j] = ((BaselineXValue1Temp[i] << 8) |BaselineXValue1[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X1_VALUE[%d] = %d---\n", j, BaselineXValue1[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 60);
		
		
	}

	//Read X2 baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_BASELINE_X2_VALUE, BaselineXValue2Temp, 14))
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek:  AT168_SINTEK_BASELINE_X2_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek:  AT168_SINTEK_BASELINE_X2_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_X2_VALUE[%d] = 0x%x---\n", i, BaselineXValue2Temp[i]);
			i++;

		}while(i < 14);
		#endif
		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				BaselineXValue2[j] =  BaselineXValue2Temp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((BaselineXValue1Temp[i]) & 0x80) 
				{
					BaselineXValue2[j] = ((BaselineXValue2Temp[i] << 8) |BaselineXValue2[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X2_VALUE temp 1 [%d] = %d---\n", j, BaselineXValue2[j]);
				}
				else
				{
					BaselineXValue2[j] = ((BaselineXValue2Temp[i] << 8) |BaselineXValue2[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X2_VALUE[%d] = %d---\n", j, BaselineXValue2[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 14);
	}

	//Read Y baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_BASELINE_Y_VALUE, BaselineYValueTemp, 42))
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek:  AT168_SINTEK_BASELINE_Y_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetBaselineSintek:  AT168_SINTEK_BASELINE_Y_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_Y_VALUE[%d] = 0x%x---\n", i, BaselineYValueTemp[i]);
			i++;

		}while(i < 21);
		#endif
		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				BaselineYValue[j] =  BaselineYValueTemp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((BaselineYValueTemp[i]) & 0x80) 
				{
					BaselineYValue[j] = ((BaselineYValueTemp[i] << 8) |BaselineYValue[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_Y_VALUE temp 1 [%d] = %d---\n", j, BaselineYValue[j]);
				}
				else
				{
					BaselineYValue[j] = ((BaselineYValueTemp[i] << 8) |BaselineYValue[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_BASELINE_Y_VALUE[%d] = %d---\n", j, BaselineYValue[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 42);
	}

	//from 0-29 is X1 value, 30-36 is X2 value, 37-57 is Y value
	i = 0;
	do
	{
		if((i>=0) && (i <= 29))
		{
			AT168_Capabilities.BaselineDate[i] = BaselineXValue1[i];
		}
		else if((i>=30) && (i <= 36))
		{
			AT168_Capabilities.BaselineDate[i] = BaselineXValue2[i - 30];
		}
		else if((i>=37) && (i <= 57))
		{
			AT168_Capabilities.BaselineDate[i] = BaselineYValue[i - 37];
		}
		i++;
	}while(i < 58);
	
	NvOsDebugPrintf("---Sintek BaselineDate ---\n");
	NvOsDebugPrintf("BaselineDate X Value is--\n");
	i = 0;
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.BaselineDate[i]);
		i++;
	}while(i < 37);
	NvOsDebugPrintf("\n");

	i = 37;
	NvOsDebugPrintf("BaselineDate Y Value is--\n");
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.BaselineDate[i]);
		i++;
	}while(i < 58);
	NvOsDebugPrintf("\n");
	
}

void AT168_SetBaselineCandoTango(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	NvOsDebugPrintf("AT168_SetBaselineCandoTango is Cando Tango IC \n");

	//Cando two Tango IC have 37 X line(30 in IC 1 / 7 in IC 2) and 21 Y line;
	//Cando have three buf from touchscreen,
	//  1  pin assignment;  2  value;   3  sign +/-;

	NvU8 BaselineTemp[128] = {0};

	NvU8 BaselineValueTemp[57] = {0};
	NvU8 BaselineSignTemp[57] = {0};
	
	NvS16 BaselineXValue[37] = {0};
	NvS16 BaselineYValue[20] = {0};

	NvU8 TP_LoginBaselineCommand[1];
	TP_LoginBaselineCommand[0] = AT168_CANDO_BASELINE_COMMAND;
	NvU8 i = 0;
	NvU8 j = 0;
	NvU8 k = 0;
	NvU8 count = 0;
	NvBool bGetValue  = NV_FALSE;
	NvBool bGetSign  = NV_FALSE;
	NvBool bGetValueSign  = NV_FALSE;
	
	do{
CandoTangoRetryWriteBaselineCommand:
		if(!(AT168_Bootloader_Write(hTouch, TP_LoginBaselineCommand, 1))){
			if(k < 60){
				k++;
				NvOsDebugPrintf("AT168_SetBaselineCandoTango CANDO_BASELINE_COMMAND fail num is (%d) \n", k);
				msleep(5);
				goto CandoTangoRetryWriteBaselineCommand;
			}else{
				NvOsDebugPrintf("AT168_SetBaselineCandoTango Write CANDO_BASELINE_COMMAND fail \n");
				return;
			}
		}else{
			NvOsDebugPrintf("AT168_SetBaselineCandoTango Write CANDO_BASELINE_COMMAND OK \n");
		}
	
		msleep(100);

		if(!(AT168_Bootloader_Read(hTouch, BaselineTemp, 128)))
		{
			NvOsDebugPrintf("AT168_SetBaselineCandoTango  AT168_Bootloader_Read fail \n");
		}
		else
		{
			NvOsDebugPrintf("AT168_SetBaselineCandoTango  AT168_Bootloader_Read OK \n");

			j= 0;
			#if 0
			do
			{
				NvOsDebugPrintf("AT168_SetBaselineCandoTango: BaselineTemp[%d] = %d---\n", j, BaselineTemp[j]);
				j++;

			}while(j < 128);
			#endif
			j= 0;

			//get the data from BaselineTemp to output buf
			//--------------------------------------------------------//
			#if 1
			if( 0xD1 == BaselineTemp[1] ){
				NvOsDebugPrintf("AT168_SetBaselineCandoTango  BaselineTemp[1] is 0XD1 \n");
			}else if( 0xD2 == BaselineTemp[1] ){
				NvOsDebugPrintf("AT168_SetBaselineCandoTango  BaselineTemp[1] is 0XD2 \n");
				do
				{
					BaselineValueTemp[i] = BaselineTemp[i+3];
					i++;
				}while(i < 37);

				do
				{
					BaselineValueTemp[i] = BaselineTemp[i+4];
					i++;
				}while(i < 57);

				#if 0
				do
				{
					NvOsDebugPrintf("AT168_SetBaselineCandoTango: BaselineValueTemp[%d] = %d---\n", j, BaselineValueTemp[j]);
					j++;

				}while(j < 57);
				#endif
				j= 0;
				
				bGetValue = NV_TRUE;
			}else if( 0xD3 == BaselineTemp[1] ){
				NvOsDebugPrintf("AT168_SetBaselineCandoTango  BaselineTemp[1] is 0XD3 \n");
				do
				{
					BaselineSignTemp[i] = BaselineTemp[i+3];
					i++;
				}while(i < 37);

				do
				{
					BaselineSignTemp[i] = BaselineTemp[i+4];
					i++;
				}while(i < 57);

				#if 0
				do
				{
					NvOsDebugPrintf("AT168_SetBaselineCandoTango: BaselineSignTemp[%d] = %d---\n", j, BaselineSignTemp[j]);
					j++;

				}while(j < 57);
				#endif
				j= 0;
				
				bGetSign = NV_TRUE;
			}else{
				NvOsDebugPrintf("AT168_SetBaselineCandoTango  BaselineTemp[1] error data \n");
			}
			
			
			if((NV_TRUE == bGetValue) && (NV_TRUE == bGetSign)){
				NvOsDebugPrintf("AT168_SetBaselineCandoTango already get value and sign from TS \n");
				i = 0;
				do
				{
					if((BaselineSignTemp[i] == 0) ||(BaselineValueTemp[i] == 0)){
						BaselineXValue[i] = BaselineValueTemp[i];
					}else{
						BaselineXValue[i] = ((32768 - BaselineValueTemp[i]) | 0x8000);
						//BaselineXValue[i] = BaselineValueTemp[i];
					}
					i++;
				}while(i < 37);
				i = 0;
				do
				{
					if((BaselineSignTemp[i + 37] == 0) ||(BaselineValueTemp[i + 37] == 0)){
						BaselineYValue[i] = BaselineValueTemp[i + 37];
					}else{
						BaselineYValue[i] = ((32768 - BaselineValueTemp[i + 37]) | 0x8000);
						//BaselineYValue[i] = BaselineValueTemp[i + 37];
					}
					i++;
				}while(i < 20);

				j= 0;
				do
				{
					NvOsDebugPrintf("AT168_SetBaselineCandoTango: BaselineXValue[%d] = %d---\n", j, BaselineXValue[j]);
					j++;

				}while(j < 37);
				
				j= 0;
				do
				{
					NvOsDebugPrintf("AT168_SetBaselineCandoTango: BaselineYValue[%d] = %d---\n", j, BaselineYValue[j]);
					j++;

				}while(j < 20);
				
				bGetValueSign = NV_TRUE;
				count = 2;
				
			}
			i = 0;
			#endif
			count ++;
			//--------------------------------------------------------//
		}
	}
	//while(NV_FALSE == bGetValueSign );
	while(count < 3 );

	// Reset TP
	NvU8 nLoop = 0;
	NvU8 ResetTPCommand[1];
	ResetTPCommand[0] = 0xc5;

	nLoop = 0;
	while(nLoop<5)
	{
		if(!(AT168_Bootloader_Write(hTouch, ResetTPCommand, 1)))
		{
			NvOsDebugPrintf("AT168_SetBaselineCandoTango  AT168_Bootloader_Write  ResetTPCommand fail nLoop is %d \n", nLoop);
			nLoop ++;
			msleep(3);
		}
		else
		{
			NvOsDebugPrintf("AT168_SetBaselineCandoTango  AT168_Bootloader_Write  ResetTPCommand success \n");
			break;
		}
	}


	i = 0;
	do
	{
		AT168_Capabilities.BaselineDate[i] = BaselineXValue[i];
		i++;
	}while(i < 37);
	i = 37;
	do
	{
		AT168_Capabilities.BaselineDate[i] = BaselineYValue[i - 37];
		i++;
	}while(i < 57);
	
	i = 0;
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.BaselineDate[i]);
		i++;
	}while(i < 37);
	NvOsDebugPrintf("\n");

	i = 37;
	NvOsDebugPrintf("BaselineDate Y Value is--\n");
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.BaselineDate[i]);
		i++;
	}while(i < 57);

	
}

void AT168_SetBaseline(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	
	if(AT168_Capabilities.Version & 0x02000000){
		AT168_SetBaselineSintekTango(hDevice);
	}else if((AT168_Capabilities.Version & 0x01000000) || (AT168_Capabilities.Version & 0x04000000)){
		AT168_SetBaselineCandoTango(hDevice);
	}else{
		NvOsDebugPrintf("waring : AT168_SetBaseline not IC have been found \n");
	}

	//Copy AT168_Capabilities data to hTouch->Caps
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
}

void AT168_SetCalibrateResultSintekTango(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango is Sintek Tango IC \n");

	int i = 0;
	int j = 0;
	NvBool isodd = NV_TRUE;

	if(!(AT168_WRITE(hTouch, AT168_INTERNAL_ENABLE, AT168_INTERNAL_ENABLE_VALUE)))
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango  AT168_INTERNAL_ENABLE_VALUE fail \n");
	}
	else
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango  AT168_INTERNAL_ENABLE_VALUE OK \n");
	}

	msleep(100);

	//Sintek two Tango IC have 37 X line(30 in IC 1 / 7 in IC 2) and 21 Y line;
	NvU8 CalibrateResultXValue1Temp[60] = {0};
	NvU8 CalibrateResultXValue2Temp[14] = {0};
	NvU8 CalibrateResultYValueTemp[42] = {0};

	NvS16 CalibrateResultXValue1[30] = {0};
	NvS16 CalibrateResultXValue2[7] = {0};
	NvS16 CalibrateResultYValue[21] = {0};

	//Read X1 baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_CALIBRATERESULT_X1_VALUE, CalibrateResultXValue1Temp, 60))
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_X1_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_X1_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_X1_VALUE temp [%d] = 0x%x---\n", i, BaselineXValue1Temp[i]);
			i++;

		}while(i < 60);
		#endif

		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				CalibrateResultXValue1[j] =  CalibrateResultXValue1Temp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((CalibrateResultXValue1Temp[i]) & 0x80) 
				{
					CalibrateResultXValue1[j] = ((CalibrateResultXValue1Temp[i] << 8) |CalibrateResultXValue1[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X1_VALUE temp 1 [%d] = %d---\n", j, BaselineXValue1[j]);
				}
				else
				{
					CalibrateResultXValue1[j] = ((CalibrateResultXValue1Temp[i] << 8) |CalibrateResultXValue1[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_CALIBRATERESULT_X1_VALUE[%d] = %d---\n", j, CalibrateResultXValue1[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 60);
		
		
	}

	//Read X2 baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_CALIBRATERESULT_X2_VALUE, CalibrateResultXValue2Temp, 14))
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_X2_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_X2_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_X2_VALUE[%d] = 0x%x---\n", i, BaselineXValue2Temp[i]);
			i++;

		}while(i < 14);
		#endif
		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				CalibrateResultXValue2[j] =  CalibrateResultXValue2Temp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((CalibrateResultXValue2Temp[i]) & 0x80) 
				{
					CalibrateResultXValue2[j] = ((CalibrateResultXValue2Temp[i] << 8) |CalibrateResultXValue2[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_X2_VALUE temp 1 [%d] = %d---\n", j, BaselineXValue2[j]);
				}
				else
				{
					CalibrateResultXValue2[j] = ((CalibrateResultXValue2Temp[i] << 8) |CalibrateResultXValue2[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_CALIBRATERESULT_X2_VALUE[%d] = %d---\n", j, CalibrateResultXValue2[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 14);
	}

	//Read Y baseline data
	if(!AT168_READ(hTouch, AT168_SINTEK_CALIBRATERESULT_Y_VALUE, CalibrateResultYValueTemp, 42))
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_Y_VALUE fail .\n");
		return NV_FALSE;
	}
	else
	{
		NvOsDebugPrintf("AT168_SetCalibrateResultSintekTango:  AT168_Open AT168_SINTEK_CALIBRATERESULT_Y_VALUE success .\n");

		#if 0
		i = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: AT168_SINTEK_BASELINE_Y_VALUE[%d] = 0x%x---\n", i, BaselineYValueTemp[i]);
			i++;

		}while(i < 21);
		#endif
		i = 0;
		j = 0;
		isodd = NV_TRUE;
		do
		{
			if( NV_TRUE == isodd )
			{
				CalibrateResultYValue[j] =  CalibrateResultYValueTemp[i];
				isodd = NV_FALSE;
			}
			else
			{
				if((CalibrateResultYValueTemp[i]) & 0x80) 
				{
					CalibrateResultYValue[j] = ((CalibrateResultYValueTemp[i] << 8) |CalibrateResultYValue[j]);
					//NvOsDebugPrintf("AT168_SINTEK_BASELINE_Y_VALUE temp 1 [%d] = %d---\n", j, BaselineYValue[j]);
				}
				else
				{
					CalibrateResultYValue[j] = ((CalibrateResultYValueTemp[i] << 8) |CalibrateResultYValue[j]);
				}
				//NvOsDebugPrintf("AT168_SINTEK_CALIBRATERESULT_Y_VALUE[%d] = %d---\n", j, CalibrateResultYValue[j]);
				j++;
				isodd = NV_TRUE;
			}
			i++;
		}while(i < 42);
	}

	//from 0-29 is X1 value, 30-36 is X2 value, 37-57 is Y value
	i = 0;
	do
	{
		if((i>=0) && (i <= 29))
		{
			AT168_Capabilities.CalibrateResultDate[i] = CalibrateResultXValue1[i];
		}
		else if((i>=30) && (i <= 36))
		{
			AT168_Capabilities.CalibrateResultDate[i] = CalibrateResultXValue2[i - 30];
		}
		else if((i>=37) && (i <= 57))
		{
			AT168_Capabilities.CalibrateResultDate[i] = CalibrateResultYValue[i - 37];
		}
		i++;
	}while(i < 58);
	
	NvOsDebugPrintf("--- CalibrateResultDate ---\n");
	NvOsDebugPrintf("CalibrateResultDate X Value is--\n");
	i = 0;
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.CalibrateResultDate[i]);
		i++;
	}while(i < 37);
	NvOsDebugPrintf("\n");

	i = 37;
	NvOsDebugPrintf("CalibrateResultDate Y Value is--\n");
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.CalibrateResultDate[i]);
		i++;
	}while(i < 58);
	NvOsDebugPrintf("\n");
	
}

void AT168_SetCalibrateResultCandoTango(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango is Cando Tango IC \n");

	//Cando two Tango IC have 37 X line(30 in IC 1 / 7 in IC 2) and 21 Y line;
	//Cando have three buf from touchscreen,
	//  1  pin assignment;  2  value;   3  sign +/-;

	NvU8 CalibrateResultTemp[128] = {0};

	NvU8 CalibrateResultValueTemp[57] = {0};
	NvU8 CalibrateResultSignTemp[57] = {0};
	
	NvS16 CalibrateResultXValue[37] = {0};
	NvS16 CalibrateResultYValue[20] = {0};

	NvU8 TP_LoginCalibrateResultCommand[1];
	TP_LoginCalibrateResultCommand[0] = AT168_CANDO_CALIBRATERESULT_COMMAND;
	NvU8 i = 0;
	NvU8 j = 0;
	NvU8 k = 0;
	NvU8 count = 0;
	NvBool bGetValue  = NV_FALSE;
	NvBool bGetSign  = NV_FALSE;
	NvBool bGetValueSign  = NV_FALSE;
	
	do{
CandoTangoRetryWriteCalibrateResultCommand:
		if(!(AT168_Bootloader_Write(hTouch, TP_LoginCalibrateResultCommand, 1))){
			if(k < 60){
				k++;
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango CANDO_CALIBRATERESULT_COMMAND fail num is (%d) \n", k);
				msleep(5);
				goto CandoTangoRetryWriteCalibrateResultCommand;
			}else{
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango Write CANDO_CALIBRATERESULT_COMMAND fail \n");
				return;
			}
		}else{
			NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango Write CANDO_CALIBRATERESULT_COMMAND OK \n");
		}
	
		msleep(100);

		if(!(AT168_Bootloader_Read(hTouch, CalibrateResultTemp, 128)))
		{
			NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  AT168_Bootloader_Read fail \n");
		}
		else
		{
			NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  AT168_Bootloader_Read OK \n");

			j= 0;
			#if 0
			do
			{
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango: CalibrateResultTemp[%d] = %d---\n", j, CalibrateResultTemp[j]);
				j++;

			}while(j < 128);
			#endif
			j= 0;

			//get the data from CalibrateResultTemp to output buf
			//--------------------------------------------------------//
			#if 1
			if( 0xD1 == CalibrateResultTemp[1] ){
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  CalibrateResultTemp[1] is 0XD1 \n");
			}else if( 0xD2 == CalibrateResultTemp[1] ){
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  CalibrateResultTemp[1] is 0XD2 \n");
				do
				{
					CalibrateResultValueTemp[i] = CalibrateResultTemp[i+3];
					i++;
				}while(i < 37);

				do
				{
					CalibrateResultValueTemp[i] = CalibrateResultTemp[i+4];
					i++;
				}while(i < 57);

				#if 0
				do
				{
					NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango: CalibrateResultValueTemp[%d] = %d---\n", j, CalibrateResultValueTemp[j]);
					j++;

				}while(j < 57);
				#endif
				j= 0;
				
				bGetValue = NV_TRUE;
			}else if( 0xD3 == CalibrateResultTemp[1] ){
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  CalibrateResultTemp[1] is 0XD3 \n");
				do
				{
					CalibrateResultSignTemp[i] = CalibrateResultTemp[i+3];
					i++;
				}while(i < 37);

				do
				{
					CalibrateResultSignTemp[i] = CalibrateResultTemp[i+4];
					i++;
				}while(i < 57);

				#if 0
				do
				{
					NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango: CalibrateResultSignTemp[%d] = %d---\n", j, CalibrateResultSignTemp[j]);
					j++;

				}while(j < 57);
				#endif
				j= 0;
				
				bGetSign = NV_TRUE;
			}else{
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  CalibrateResultTemp[1] error data \n");
			}
			
			
			if((NV_TRUE == bGetValue) && (NV_TRUE == bGetSign)){
				NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango already get value and sign from TS \n");
				i = 0;
				do
				{
					if((CalibrateResultSignTemp[i] == 0) ||(CalibrateResultValueTemp[i] == 0)){
						CalibrateResultXValue[i] = CalibrateResultValueTemp[i];
					}else{
						CalibrateResultXValue[i] = ((32768 - CalibrateResultValueTemp[i]) | 0x8000);
						//CalibrateResultXValue[i] = CalibrateResultValueTemp[i];
					}
					i++;
				}while(i < 37);
				i = 0;
				do
				{
					if((CalibrateResultSignTemp[i + 37] == 0) ||(CalibrateResultValueTemp[i + 37] == 0)){
						CalibrateResultYValue[i] = CalibrateResultValueTemp[i + 37];
					}else{
						CalibrateResultYValue[i] = ((32768 - CalibrateResultValueTemp[i + 37]) | 0x8000);
						//CalibrateResultYValue[i] = CalibrateResultValueTemp[i + 37];
					}
					i++;
				}while(i < 20);

				j= 0;
				do
				{
					NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango: CalibrateResultXValue[%d] = %d---\n", j, CalibrateResultXValue[j]);
					j++;

				}while(j < 37);
				
				j= 0;
				do
				{
					NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango: CalibrateResultYValue[%d] = %d---\n", j, CalibrateResultYValue[j]);
					j++;

				}while(j < 20);
				
				bGetValueSign = NV_TRUE;
				count = 2;
				
			}
			i = 0;
			#endif
			count ++;
			//--------------------------------------------------------//
		}
	}
	//while(NV_FALSE == bGetValueSign );
	while(count < 3 );

	// Reset TP
	NvU8 nLoop = 0;
	NvU8 ResetTPCommand[1];
	ResetTPCommand[0] = 0xc5;

	nLoop = 0;
	while(nLoop<5)
	{
		if(!(AT168_Bootloader_Write(hTouch, ResetTPCommand, 1)))
		{
			NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  AT168_Bootloader_Write  ResetTPCommand fail nLoop is %d \n", nLoop);
			nLoop ++;
			msleep(3);
		}
		else
		{
			NvOsDebugPrintf("AT168_SetCalibrateResultCandoTango  AT168_Bootloader_Write  ResetTPCommand success \n");
			break;
		}
	}

	NvOsDebugPrintf("--- CalibrateResultDate ---\n");
	NvOsDebugPrintf("CalibrateResultDate X Value is--\n");
	i = 0;
	do
	{
		AT168_Capabilities.CalibrateResultDate[i] = CalibrateResultXValue[i];
		i++;
	}while(i < 37);
	i = 37;
	do
	{
		AT168_Capabilities.CalibrateResultDate[i] = CalibrateResultYValue[i - 37];
		i++;
	}while(i < 57);
	
	i = 0;
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.CalibrateResultDate[i]);
		i++;
	}while(i < 37);
	NvOsDebugPrintf("\n");

	i = 37;
	NvOsDebugPrintf("CalibrateResultDate Y Value is--\n");
	do
	{
		NvOsDebugPrintf(" %d ", AT168_Capabilities.CalibrateResultDate[i]);
		i++;
	}while(i < 57);
	NvOsDebugPrintf("\n");
	
}

void AT168_SetCalibrateResult(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	if(AT168_Capabilities.Version & 0x02000000){
		AT168_SetCalibrateResultSintekTango(hDevice);
	}else if((AT168_Capabilities.Version & 0x01000000) || (AT168_Capabilities.Version & 0x04000000)){
		AT168_SetCalibrateResultCandoTango(hDevice);
	}else{
		NvOsDebugPrintf("waring : AT168_SetCalibrateResult not IC have been found \n");
	}

	//Copy AT168_Capabilities data to hTouch->Caps
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
}

NvBool AT168_BurnSintexBootloader(NvOdmTouchDeviceHandle hDevice)
{
	NvOsDebugPrintf("AT168_BurnSintexBootloader begin \n");
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	NvU8 ResetNum = 0;
	NvU8 status[4];

	const char *filename_path[10] = 
  		{"/data/SintekBootloader",
  		"/mnt/sdcard/SintekBootloader",
  		"/mnt/sdcard1/SintekBootloader",
  		"/mnt/sdcard2/SintekBootloader",
  		"/mnt/sdcard/sdcard1/SintekBootloader",
  		"/mnt/sdcard/sdcard2/SintekBootloaderi",
  		"/mnt/sdcard/sdcard3/SintekBootloader",
  		"/mnt/sdcard/udisk1/SintekBootloader",
  		"/mnt/sdcard/udisk2/SintekBootloader",
  		"/mnt/usbdisk/SintekBootloader"
  		};

	struct file *filp;
	struct inode *inode; 
	mm_segment_t fs; 
	off_t fsize; 
	
	NvU8 *buf; 
	NvU8 sendbuf[144]; 
	NvBool isodd = NV_TRUE;
	int i = 0, j = 0;
	int k = 0;
	
	unsigned long magic; 
	NvU32 PinStateValue;
	NvU8 crc_value[1];
	
	printk("start download SintekBootloader \n"); 

	fs=get_fs(); 
	set_fs(KERNEL_DS); 
	
	if((IS_ERR(filp = filp_open(filename_path[0],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[1],O_RDONLY|O_LARGEFILE,0))) 
		&& (IS_ERR(filp = filp_open(filename_path[2],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[3],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[4],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[5],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[6],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[7],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[8],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[9],O_RDONLY|O_LARGEFILE,0)))
	)
	{
		printk("can not open file SintekBootloader , please check the file path \r\n");
		return NV_FALSE;
	}
	else
	{
		printk("Open file SintekBootloader success \r\n");
		inode=filp->f_dentry->d_inode; 
		magic=inode->i_sb->s_magic; 
		AT168_PRINTF(("<1>file system magic:%li \n",magic)); 
		AT168_PRINTF(("<1>super blocksize:%li \n",inode->i_sb->s_blocksize)); 
		AT168_PRINTF(("<1>inode %li \n",inode->i_ino));
		fsize=inode->i_size; 
		printk("Sintek bootloader file size:%d \n",fsize);
		buf=(NvU8 *) kmalloc(fsize+1,GFP_ATOMIC); 
		filp->f_op->read(filp, buf, fsize,&(filp->f_pos));
		filp_close(filp,NULL);
	}

	set_fs(fs);

RetryBurnSintek:
	//************************************************************************************//
	//Re-start the touch panel,  then goto bootloader status
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_Low);
	msleep(5);
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_High);
	msleep(20);	//must send bootloader I2C address after reset in 2ms-50ms
	
	if(!(AT168_Bootloader_Read(hTouch, status, 4)))
	{
		ResetNum ++;
		if(ResetNum > 3)
		{
			NvOsDebugPrintf("AT168_BurnSintexBootloader --  Read status fail  return NV_FALSE \n");
			return NV_FALSE;
		}
		else
		{
			NvOsDebugPrintf("AT168_BurnSintexBootloader --  Read status fail  try %d \n", ResetNum);
			goto RetryBurnSintek;
		}
	}
	else
	{
		NvOsDebugPrintf("AT168_BurnSintexBootloader --Read status OK  ( 0x%x 0x%x 0x%x 0x%x ) \n", status[0], status[1], status[2], status[3]);
	}
	//************************************************************************************//
	k = 0;
	//printk("<1>The File Content  is:\n");
	for(i = 1;i < fsize-1; i++)		//ignore the first '$', so i =1
	{
		if(('$' == buf[i]) ||(0x0d == buf[i]) || (0x0a == buf[i]))
		{
			if('$' == buf[i])
			{
				//printk(" $  buf[%d] = 0x%x \n",i, buf[i]);
				//printk(" j = %d \n",j);
				j = 0;
				
			}
			else
			{
				//printk(" buf go to line end \n");
			}
		}
		else
		{
			if(NV_TRUE == isodd)
			{
				sendbuf[j] = 0;
				if(('0'<=buf[i])&&(buf[i]<='9'))
				{
					sendbuf[j] = 16*(buf[i] - 0x30);
				}
				else if(('A'<=buf[i])&&(buf[i]<='F'))
				{
					sendbuf[j] = 16*(buf[i] - 'A' + 0x0a);
				}
				else
				{
					printk(" odd  NV_FALSE buf[%d] = 0x%x \n",i, buf[i]);
					goto RetryBurnSintek;
					//return NV_FALSE;
				}
				isodd = NV_FALSE;
				//printk("odd  buf[%d] = 0x%x  --- sendbuf [%d] = 0x%x \n",i, buf[i], j, sendbuf[j]);
			}
			else //even
			{
				if(('0'<=buf[i])&&(buf[i]<='9'))
				{
					sendbuf[j] += (buf[i] - 0x30);
				}
				else if(('A'<=buf[i])&&(buf[i]<='F'))
				{
					sendbuf[j] += (buf[i] - 'A' + 0x0a);
				}
				else
				{
					printk(" even NV_FALSE buf[%d] = 0x%x \n",i, buf[i]);
					goto RetryBurnSintek;
					//return NV_FALSE;
				}
				isodd = NV_TRUE;
				//printk("even  buf[%d] = 0x%x  --- sendbuf [%d] = 0x%x \n",i, buf[i], j, sendbuf[j]);
				#if 0
				if(sendbuf[j] > 0x0f)
				{
					printk("%x",sendbuf[j]);
				}
				else
				{
					printk("0%x",sendbuf[j]);
				}
				#endif
				
				j++;

				//Third step : write bootloader command, download 143 bytes every time
				
				if(143 == j)
				{
					//printk("\n");
					//printk("sendbuf line %d is ",k);
					k++;

					if(!(AT168_Bootloader_Write(hTouch, sendbuf, 143)))
					{
						NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  AT168_Bootloader_Write fail \n");
						goto RetryBurnSintek;
						//return NV_FALSE;
					}
					else
					{
						//NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  AT168_Bootloader_Write success \n");
					}

					if(0x01 != sendbuf[0])	//0x01 is the last line
					{
						//NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  sendbuf[0] is not 0x01 \n");
						msleep(1);
						do
						{
							//NvOsDebugPrintf("AT168_BurnBootloader step 3  NvOdmGpioGetState begin \n");
							//msleep(2);
							NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
							//msleep(2);
							//NvOsDebugPrintf("AT168_BurnBootloader step 3  NvOdmGpioGetState end \n");
						}while(PinStateValue);
						msleep(1);
					//}

					//CRC check
					//NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  CRC check begin \n");
					if(!(AT168_Bootloader_Read(hTouch, crc_value, 1)))
					{
						NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  AT168_Bootloader_Read fail \n");
						goto RetryBurnSintek;
						//return NV_FALSE;
					}
					else
					{
						NvOsDebugPrintf("AT168_BurnSintexBootloader step 3 line %d return the crc_value[0] is 0x%x \n", k, crc_value[0]);
						if((crc_value[0] >= 0x80) && (0x01 != sendbuf[0]))  // if first bit is '1', crc value is fail
						{
							NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  CRC  fail j = %d\n",j);
							printk("sendbuf line %d is ",k);
							int ii = 0;
							do
							{
								if(sendbuf[ii] > 0x0f)
								{
									printk("%x",sendbuf[ii]);
								}
								else
								{
									printk("0%x",sendbuf[ii]);
								}
								ii++;
							}while(ii < j);
							printk("\n");
							
							goto RetryBurnSintek;
							//return NV_FALSE;;           // return Fail
						}
						else
						{
							//NvOsDebugPrintf("AT168_BurnSintexBootloader step 3  CRC  success \n");
						}
					}
					}
				}
			}
		}
	}
	
	msleep(3000);
	
	//Fourth step : Force reset and calibration
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_Low);
	msleep(5);
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_High);
	msleep(60);
	//do calibration
	hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
	if(AT168_WRITE(hTouch, AT168_SPECOP, AT168_SPECOP_CALIBRATION_VALUE))
	{
		NvOsDebugPrintf("AT168_BurnSintexBootloader do calibration OK .\n");
	}
	else
	{
		NvOsDebugPrintf("AT168_BurnSintexBootloader do calibration fail .\n");
	}
	
	return NV_TRUE;
}
NvBool AT168_BurnCandoBootloader(NvOdmTouchDeviceHandle hDevice)
{
	
	NvOsDebugPrintf("AT168_BurnCandoBootloader begin \n");
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	//**************************************************************************************//
	//Second step : prepare before write to touch panel
	//------ goto the Cando bootloader mode ------
	NvU16 g_CRCReceived;		//for check the local_crc
	NvU32 PinStateValue;		//for check the int gpio state
	NvU8	readbuff[9];
	NvU8	checkintstatusnum = 0;
	
	// Step 0: Reboot TP boot-loader
	NvU8 TP_ReBootLoadCommand[1];
	TP_ReBootLoadCommand[0] = 0xC0; //command : TP_ReBootLoad
	NvU8 RetryC0Num = 0;
RetryBurnCando:
	if(!(AT168_Bootloader_Write(hTouch, TP_ReBootLoadCommand, 1)))
	{
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.1  AT168_Bootloader_Write 0xC0 fail \n");
		if(RetryC0Num > 100)
			return NV_FALSE;
		else
			RetryC0Num++;
		goto RetryBurnCando;
	}
	else
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.1  AT168_Bootloader_Write 0xC0 success \n"));
	}
	msleep(20);	//delay 1ms for process to be completed

	//**************************************************************************************//

	//------ Enable FLASH writes and erases ------
	NvBool TangoIICAddress = NV_TRUE;
	NvU8 nLoop;
	NvU8 FlashKeycommand[4];
	FlashKeycommand[0] = 3;	//BLSize
	FlashKeycommand[1] = 0xF7;	//BL_SetFlashKeyCodes
	FlashKeycommand[2] = 0xA5; 	//FLASH_KEY0
	FlashKeycommand[3] = 0xF1;  //FLASH_KEY1
		
RetryBurnRM31060:
	nLoop = 0;

	while(nLoop<5)
	{
		if(!(AT168_Bootloader_Write(hTouch, FlashKeycommand, 4)))
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.2  AT168_Bootloader_Write FlashKeycommand looping time %d \n", nLoop);
			nLoop++;
		}
		else
		{
			AT168_PRINTF(("AT168_BurnCandoBootloader step 2.2  AT168_Bootloader_Write  FlashKeycommand success \n"));
			break;
		}
		
		if(( nLoop == 5 ) && ( NV_TRUE == TangoIICAddress ))
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.2  AT168_Bootloader_Write 0x5c FlashKeycommand fail \n");
			hTouch->DeviceAddr = (NvU32)(0x5e << 1);
			TangoIICAddress = NV_FALSE;
			goto RetryBurnRM31060;
		}
		else if(( nLoop == 5 ) && ( NV_FALSE == TangoIICAddress )){
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.2  AT168_Bootloader_Write 0x5e FlashKeycommand fail \n");
			hTouch->DeviceAddr = (NvU32)(0x5c << 1);
			goto RetryBurnCando;
		}
	}
	
	//wait for gpio_int change to high
	msleep(1);
	checkintstatusnum = 0;
	do
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.2  NvOdmGpioGetState begin \n"));
		msleep(2);
		NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.2  NvOdmGpioGetState end \n"));
		checkintstatusnum+=2;
		if(checkintstatusnum > 5000)
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.2  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
			return NV_FALSE;
		}
	}while(!PinStateValue);
	msleep(1);
	
	//**************************************************************************************//
	//First step : read the bootloader source file
	const char *filename_path[10] = 
  		{"/data/CandoBootloader",
  		"/mnt/sdcard/CandoBootloader",
  		"/mnt/sdcard1/CandoBootloader",
  		"/mnt/sdcard2/CandoBootloader",
  		"/mnt/sdcard/sdcard1/CandoBootloader",
  		"/mnt/sdcard/sdcard2/CandoBootloader",
  		"/mnt/sdcard/sdcard3/CandoBootloader",
  		"/mnt/sdcard/udisk1/CandoBootloader",
  		"/mnt/sdcard/udisk2/CandoBootloader",
  		"/mnt/usbdisk/CandoBootloader"
  		};
	
	struct file *filp; 
	struct inode *inode; 
	mm_segment_t fs; 
	off_t fsize; 
	unsigned long magic;
	NvU8 *buf; 

	NvBool isodd = NV_TRUE;
	NvU16 linenum;
	//int i = 0, j = 0;

	printk("start download CandoBootloader"); 

	fs=get_fs(); 
	set_fs(KERNEL_DS); 

	if((IS_ERR(filp = filp_open(filename_path[0],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[1],O_RDONLY|O_LARGEFILE,0))) 
		&& (IS_ERR(filp = filp_open(filename_path[2],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[3],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[4],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[5],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[6],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[7],O_RDONLY|O_LARGEFILE,0)))
		&& (IS_ERR(filp = filp_open(filename_path[8],O_RDONLY|O_LARGEFILE,0))) && (IS_ERR(filp = filp_open(filename_path[9],O_RDONLY|O_LARGEFILE,0)))
	)
	{
		printk("can not open file CandoBootloader , please check the file path \r\n");
		return NV_FALSE;
	}
	else
	{
		printk("Open file CandoBootloader success \r\n");
		inode=filp->f_dentry->d_inode; 
		magic=inode->i_sb->s_magic; 
		AT168_PRINTF(("<1>file system magic:%li \n",magic)); 
		AT168_PRINTF(("<1>super blocksize:%li \n",inode->i_sb->s_blocksize)); 
		AT168_PRINTF(("<1>inode %li \n",inode->i_ino));
		fsize=inode->i_size; 
		printk("Cando bootloader file size:%d \n",fsize);
		buf=(NvU8 *) kmalloc(fsize+1,GFP_ATOMIC); 
		filp->f_op->read(filp, buf, fsize,&(filp->f_pos));
		filp_close(filp,NULL);
	}
	
	set_fs(fs);
	
	//**************************************************************************************//
	//------ Erase last user page to clear the Validation Signature (page containing address 0x7800) ------
	NvU8 Erasecommand[4];
	Erasecommand[0] = 3;	//BLSize
	Erasecommand[1] = 0xF1;	//BL_ErasePage
	Erasecommand[2] = 0x7800 & 0xFF; 	//(eraseAddress&0xFF)
	Erasecommand[3] = 0x7800 >> 8; 	//(eraseAddress>>8)

	if(!(AT168_Bootloader_Write(hTouch, Erasecommand, 4)))
	{
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.3  AT168_Bootloader_Write  Erasecommand fail \n");
		goto RetryBurnCando;
		//return NV_FALSE;
	}
	else
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.3  AT168_Bootloader_Write  Erasecommand success \n"));
	}
	
	//wait for gpio_int change to high
	msleep(1);
	checkintstatusnum = 0;
	do
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.3  NvOdmGpioGetState begin \n"));
		msleep(2);
		NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
		AT168_PRINTF(("AT168_BurnCandoBootloader step 2.3  NvOdmGpioGetState end \n"));
		checkintstatusnum+=2;
		if(checkintstatusnum > 5000)
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 2.3  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
			return NV_FALSE;
		}
	}while(!PinStateValue);
	msleep(1);
	
	//**************************************************************************************//
	//Third step : write FW to touch panel,  Erase/Write/CRC Flash pages
	int k = 0;
	NvU8 nPageNum;
	NvU16 nWriteAddress;
	NvU8 WriteEraseCommand[4];
	NvU8 WriteCommand[36];
	NvU16 nByteWriteOffset = 0;
	NvU8 index = 0;
	NvU16 local_crc = 0;
	
	for (nPageNum = 0x04; nPageNum <= 0x1E; nPageNum++) // TP_FW_BEGIN_PAGE: 0x04 ;   TP_FW_END_PAGE : 0x1E;
	{
		nWriteAddress = (nPageNum * 1024);
		
		// Erase page before writing to it
		WriteEraseCommand[0] = 3;	//BLSize
		WriteEraseCommand[1] = 0xF1;	//BL_ErasePage
		WriteEraseCommand[2] = nWriteAddress & 0xFF; 	//(nWriteAddress&0xFF)
		WriteEraseCommand[3] = nWriteAddress >> 8; 	//(nWriteAddress>>8)

		if(!(AT168_Bootloader_Write(hTouch, WriteEraseCommand, 4)))
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.1  AT168_Bootloader_Write  WriteEraseCommand fail \n");
			goto RetryBurnCando;
			//return NV_FALSE;
		}
		else
		{
			AT168_PRINTF(("AT168_BurnCandoBootloader step 3.1  AT168_Bootloader_Write  WriteEraseCommand success \n"));
		}
		//wait for gpio_int change to high
		msleep(1);
		checkintstatusnum = 0;
		do
		{
			AT168_PRINTF(("AT168_BurnCandoBootloader step 3.1  Erase NvOdmGpioGetState begin \n"));
			msleep(2);
			NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
			AT168_PRINTF(("AT168_BurnCandoBootloader step 3.1  Erase NvOdmGpioGetState end \n"));
			checkintstatusnum+=2;
			if(checkintstatusnum > 5000)
			{
				NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.1 Erase  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
				return NV_FALSE;
			}
		}while(!PinStateValue);
		
		//get the return from TP
		memset(readbuff, 0x00, 9);
		if(!(AT168_Bootloader_Read(hTouch, readbuff, 9)))
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.1 Write  AT168_Bootloader_Read fail \n");
			goto RetryBurnCando;
			//return NV_FALSE;
		}
		else
		{
			if(0x72 == readbuff[1])	// BLR_WriteBytes : 0x72
			{
				AT168_PRINTF(("AT168_BurnCandoBootloader step 3.1 Write readbuff[1] is BLR_WriteBytes \n"));
				if((readbuff[0] >= 4) && (0x00 == readbuff[2]))	//COMMAND_SUCCESS : 0x00
					g_CRCReceived = (readbuff[3] | (((NvU16)readbuff[4])<<8));
				else
					g_CRCReceived = 0xFFFF;
			}
			AT168_PRINTF(("AT168_BurnCandoBootloader step 3.1 Write  return the status are 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", readbuff[0], readbuff[1], readbuff[2], readbuff[3], readbuff[4], readbuff[5], readbuff[6], readbuff[7], readbuff[8]));
		}
		msleep(1);

		nByteWriteOffset = 0;

		// Write 1024 bytes to FLASH
		while(nByteWriteOffset < 1024)
		{
			WriteCommand[0] = 35;	//BLSize
			WriteCommand[1] = 0xF2;	//BL_WriteBytes
			WriteCommand[2] = (nWriteAddress+nByteWriteOffset) & 0xFF ;
			WriteCommand[3] = (nWriteAddress+nByteWriteOffset) >>8;

			for (index = 0; index < 32; index++ )
			{
				WriteCommand[index+4] = buf[nWriteAddress+nByteWriteOffset+index];
			}

			if(!(AT168_Bootloader_Write(hTouch, WriteCommand, 36)))
			{
				NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.2  AT168_Bootloader_Write  WriteCommand fail \n");
				goto RetryBurnCando;
				//return NV_FALSE;
			}
			else
			{
				AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2  AT168_Bootloader_Write  WriteCommand success nPageNum = %x \n", nPageNum));
			}
			
			//wait for gpio_int change to high
			msleep(1);
			checkintstatusnum = 0;
			do
			{
				AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2  Write NvOdmGpioGetState begin \n"));
				msleep(2);
				NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
				AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2  Write NvOdmGpioGetState end \n"));
				checkintstatusnum+=2;
				if(checkintstatusnum > 5000)
				{
					NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.2 Write  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
					return NV_FALSE;
				}
			}while(!PinStateValue);
			msleep(1);
			//get the return from TP
			memset(readbuff, 0x00, 9);
			if(!(AT168_Bootloader_Read(hTouch, readbuff, 9)))
			{
				NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.2 Write  AT168_Bootloader_Read fail \n");
				goto RetryBurnCando;
				//return NV_FALSE;
			}
			else
			{
				if(0x72 == readbuff[1])	// BLR_WriteBytes : 0x72
				{
					AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2 Write readbuff[1] is BLR_WriteBytes \n"));
					if((readbuff[0] >= 4) && (0x00 == readbuff[2]))	//COMMAND_SUCCESS : 0x00
						g_CRCReceived = (readbuff[3] | (((NvU16)readbuff[4])<<8));
					else
						g_CRCReceived = 0xFFFF;
				}
				AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2 Write  return the status are 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", readbuff[0], readbuff[1], readbuff[2], readbuff[3], readbuff[4], readbuff[5], readbuff[6], readbuff[7], readbuff[8]));
			}

			//check the local_crc
			local_crc = 0;
			for( index = 0; index < 32; index++ )
			{
				local_crc = local_crc ^ WriteCommand[index+4];
				for(k = 0; k < 8; k++)
				{
				      if (local_crc & 0x01)
				      {
				         local_crc = local_crc >> 1;
				         local_crc ^= 0x8408;		//CRC_POLY : 0x8408
				      }
				      else
				      {
				         local_crc = local_crc >> 1;
				      }
				}
			}

			AT168_PRINTF(("AT168_BurnCandoBootloader step 3.2  local_crc is (%x)..g_CRCReceived is (%x)..... \n", local_crc, g_CRCReceived));

			if( local_crc != g_CRCReceived )
			{
				NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.2  AT168_Bootloader_Write  CRC check failed........ \n");
				goto RetryBurnCando;
				//return NV_FALSE;
			}
			
			nByteWriteOffset += 32;
		}
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 3.2  AT168_Bootloader_Write  OK nPageNum is  %x....... \n", nPageNum);
	}

	//**************************************************************************************//
	// Fourth step : Write Signature
	NvU8 WriteSignaturecommand[4];
	WriteSignaturecommand[0] = 3;	//BLSize
	WriteSignaturecommand[1] = 0xF4;		//BL_WriteValidation
	WriteSignaturecommand[2] = 0xC2;
	WriteSignaturecommand[3] = 0x3D;

	if(!(AT168_Bootloader_Write(hTouch, WriteSignaturecommand, 4)))
	{
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 4.1  AT168_Bootloader_Write  WriteSignaturecommand fail \n");
		goto RetryBurnCando;
		//return NV_FALSE;
	}
	else
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 4.1  AT168_Bootloader_Write  WriteSignaturecommand success \n"));
	}
	
	//wait for gpio_int change to high
	msleep(1);
	checkintstatusnum = 0;
	do
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 4.1  NvOdmGpioGetState begin \n"));
		msleep(2);
		NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
		AT168_PRINTF(("AT168_BurnCandoBootloader step 4.1  NvOdmGpioGetState end \n"));
		checkintstatusnum+=2;
		if(checkintstatusnum > 5000)
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 4.1  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
			return NV_FALSE;
		}
	}while(!PinStateValue);
	msleep(1);
	
	//**************************************************************************************//
	// Fifths step : Clear the Flash Key Codes
	NvU8 ClearFlashKeycommand[4];
	ClearFlashKeycommand[0] = 3;	//BLSize
	ClearFlashKeycommand[1] = 0xF7;		//BL_SetFlashKeyCodes
	ClearFlashKeycommand[2] = 0;
	ClearFlashKeycommand[3] = 0;

	if(!(AT168_Bootloader_Write(hTouch, ClearFlashKeycommand, 4)))
	{
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 5.1  AT168_Bootloader_Write  ClearFlashKeycommand fail \n");
		goto RetryBurnCando;
		//return NV_FALSE;
	}
	else
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 5.1  AT168_Bootloader_Write  ClearFlashKeycommand success \n"));
	}
	
	//wait for gpio_int change to high
	msleep(1);
	checkintstatusnum = 0;
	do
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 5.1  NvOdmGpioGetState begin \n"));
		msleep(2);
		NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
		AT168_PRINTF(("AT168_BurnCandoBootloader step 5.1  NvOdmGpioGetState end \n"));
		checkintstatusnum+=2;
		if(checkintstatusnum > 5000)
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 5.1  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
			return NV_FALSE;
		}
	}while(!PinStateValue);
	msleep(1);
	
	//**************************************************************************************//
	// Sixth step : Exit boot-loader
	NvU8 ExitBootloadercommand[2];
	ExitBootloadercommand[0] = 1;	//BLSize
	ExitBootloadercommand[1] = 0xF5;		//BL_ExitBootload

	if(!(AT168_Bootloader_Write(hTouch, ExitBootloadercommand, 2)))
	{
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 6.1  AT168_Bootloader_Write  ExitBootloadercommand fail \n");
		goto RetryBurnCando;
		//return NV_FALSE;
	}
	else
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 6.1  AT168_Bootloader_Write  ExitBootloadercommand success \n"));
	}
	
	//wait for gpio_int change to high
	msleep(1);
	checkintstatusnum = 0;
	do
	{
		AT168_PRINTF(("AT168_BurnCandoBootloader step 6.1  NvOdmGpioGetState begin \n"));
		msleep(2);
		NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
		AT168_PRINTF(("AT168_BurnCandoBootloader step 6.1  NvOdmGpioGetState end \n"));
		checkintstatusnum+=2;
		if(checkintstatusnum > 5000)
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 6.1  NvOdmGpioGetState checkintstatusnum up 5 second, give up \n");
			return NV_FALSE;
		}
	}while(!PinStateValue);
	msleep(5);
	
	//**************************************************************************************//
	// Seventh step : Reset TP
	NvU8 ResetTPcommand[2];
	ResetTPcommand[0] = 1;	//BLSize
	ResetTPcommand[1] = 0xFB;		//BL_ExitBootload

	nLoop = 0;
	while(nLoop<5)
	{
		if(!(AT168_Bootloader_Write(hTouch, ResetTPcommand, 2)))
		{
			NvOsDebugPrintf("AT168_BurnCandoBootloader step 7.1  AT168_Bootloader_Write  ResetTPcommand fail \n");
			
			//return NV_FALSE;
		}
		else
		{
			AT168_PRINTF(("AT168_BurnCandoBootloader step 7.1  AT168_Bootloader_Write  ResetTPcommand success \n"));
		}

		nLoop ++;
		msleep(3);
		NvOsDebugPrintf("AT168_BurnCandoBootloader step 7.1  AT168_Bootloader_Write  ResetTPcommand nLoop is %d \n", nLoop);
		
		if(nLoop == 5)
		{
			//Force reset
			NvOdmGpioSetState(hTouch->hGpio,
					hTouch->hPinReset,
					NvOdmGpioPinActiveState_Low);
			msleep(5);
			NvOdmGpioSetState(hTouch->hGpio,
					hTouch->hPinReset,
					NvOdmGpioPinActiveState_High);
			msleep(60);

			break;
		}
	}
	
	//**************************************************************************************//
	NvOsDebugPrintf("AT168_BurnCandoBootloader end \n");
	return NV_TRUE;
}
//For burn touchscreen bootloader
NvBool AT168_BurnBootloader(NvOdmTouchDeviceHandle hDevice)
{
	NvU8 status[4]; 
	// status[0] : report statues £¬normal is 00 
	// status[1] : report flash status as stored in eeprom[511] ,normal is  A5, sometimes(pre-burn error) is 00
	// status[2] : report the bootloader version  , normal is  03 
	// status[3] : report customer cryptographic key  , normal is  02 
	
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	
	//First step : get in the bootloader
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_Low);
	msleep(5);
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_High);
	msleep(20);	//must send bootloader I2C address after reset in 2ms-50ms

	hTouch->DeviceAddr = (NvU32)(0x5d << 1);	//change I2C  to bootloader address 0x5d
	
	if(!(AT168_Bootloader_Read(hTouch, status, 4)))
	{
		NvOsDebugPrintf("AT168_BurnBootloader --  Read status fail  maybe is Cando TS \n");
		hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
		if(!(AT168_BurnCandoBootloader(hDevice)))
		{
			hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
			return NV_FALSE;
		}
	}
	else
	{
		NvOsDebugPrintf("AT168_BurnBootloader --  Read status OK, now is Sintek TS \n");
		//NvOsDebugPrintf("AT168_BurnBootloader -- return the status are 0x%x 0x%x 0x%x 0x%x \n", status[0], status[1], status[2], status[3]);
		
		hTouch->DeviceAddr = (NvU32)(0x5d << 1);	//change I2C  to bootloader address 0x5d
		if(!(AT168_BurnSintexBootloader(hDevice)))
		{
			hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
			return NV_FALSE;
		}
	}
	
	hTouch->DeviceAddr = (NvU32)(0x5c << 1);	//change I2C back to normal 0x5c
	return NV_TRUE;
}

static void AT168_GpioIsr(void *arg)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)arg;

	/* Signal the touch thread to read the sample. After it is done reading the
	* sample it should re-enable the interrupt. */
	NvOdmOsSemaphoreSignal(hTouch->hIntSema);            
}

NvBool AT168_ReadCoordinate (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo* coord)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	NvU8 data[10] = {0};

	//Read once
	if(!AT168_READ(hTouch, AT168_TOUCH_NUM, data, (AT168_POS_Y1_HI - AT168_TOUCH_NUM + 1)))
	{
		NvOsDebugPrintf("NvOdmTouch_at168: Read Coord fail .\n");
		return NV_FALSE;
	}
	else
	{
		//AT168_PRINTF(("NvOdmTouch_at168:  AT168_ReadCoordinate success .\n"));
	}	

	coord->additionalInfo.Fingers = data[AT168_TOUCH_NUM];

	if((1 == coord->additionalInfo.Fingers) || (2 == coord->additionalInfo.Fingers))
	{
		coord->xcoord =
		coord->additionalInfo.multi_XYCoords[0][0] =
			(data[AT168_POS_X0_HI] << 8) | (data[AT168_POS_X0_LO]);

		coord->ycoord =
	        coord->additionalInfo.multi_XYCoords[0][1] =
			(data[AT168_POS_Y0_HI] << 8) | (data[AT168_POS_Y0_LO]);
		
		if(2 == coord->additionalInfo.Fingers)
		{
			coord->additionalInfo.multi_XYCoords[1][0] =
			(data[AT168_POS_X1_HI] << 8) | (data[AT168_POS_X1_LO]);

			coord->additionalInfo.multi_XYCoords[1][1] =
			(data[AT168_POS_Y1_HI] << 8) | (data[AT168_POS_Y1_LO]);
		}
		else if(1 == coord->additionalInfo.Fingers)
		{
			coord->additionalInfo.multi_XYCoords[1][0] = 0;//AT168_MIN_X;  //will be NvOdmTouchOrientation_H_FLIP
			coord->additionalInfo.multi_XYCoords[1][1] = 0;//AT168_MIN_Y;
		}
	}
	else  //Fingers is 0
	{
		coord->additionalInfo.Fingers = 0; //Please reset Fingers 0;

		coord->xcoord =
		coord->additionalInfo.multi_XYCoords[0][0] = 0;//AT168_MIN_X;
		coord->ycoord =
	        coord->additionalInfo.multi_XYCoords[0][1] = 0;//AT168_MIN_Y;

		coord->additionalInfo.multi_XYCoords[1][0] = 0;//AT168_MIN_X;
		coord->additionalInfo.multi_XYCoords[1][1] = 0;//AT168_MIN_Y;
	}
 	
	AT168_PRINTF(("==AT168_READ---FingerNum = %d  x[0]=%d y[0]=%d x[1]=%d y[1]=%d ===\n", 
				coord->additionalInfo.Fingers,
				coord->additionalInfo.multi_XYCoords[0][0], coord->additionalInfo.multi_XYCoords[0][1], 
				coord->additionalInfo.multi_XYCoords[1][0], coord->additionalInfo.multi_XYCoords[1][1]));

	
	//Set if NvOdmTouchSampleIgnore     //MAX fingers fit two
	coord->fingerstate = 0;		//Reset the fingerstate
	if ((coord->additionalInfo.Fingers !=0 ) && ( coord->additionalInfo.multi_XYCoords[0][0] <= 0 ||
                coord->additionalInfo.multi_XYCoords[0][0] >= AT168_Capabilities.XMaxPosition ||
                coord->additionalInfo.multi_XYCoords[0][1] <= 0 || 
                coord->additionalInfo.multi_XYCoords[0][1] >= AT168_Capabilities.YMaxPosition))
	{
		coord->fingerstate = NvOdmTouchSampleIgnore;
	}
	if ((coord->additionalInfo.Fingers ==2 ) && ( coord->additionalInfo.multi_XYCoords[1][0] <= 0 ||
                coord->additionalInfo.multi_XYCoords[1][0] >= AT168_Capabilities.XMaxPosition ||
                coord->additionalInfo.multi_XYCoords[1][1] <= 0 ||
                coord->additionalInfo.multi_XYCoords[1][1] >= AT168_Capabilities.YMaxPosition))
	{
		coord->fingerstate = NvOdmTouchSampleIgnore;
	}

	#if 0
	int i = 0;
	do
	{
		AT168_PRINTF(("NvOdmTouch_at168:  data[%d] = (0x%x)---\n", i, data[i]));
		i++;
	}while(i<10);
	#endif

	return NV_TRUE;

}

void AT168_GetCapabilities (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	*pCapabilities = hTouch->Caps;
}

NvBool AT168_PowerOnOff (NvOdmTouchDeviceHandle hDevice, NvBool OnOff)
{
	#if defined(CONFIG_7373C_V20)
	AT168_PRINTF(("NvOdm Touch: AT168_PowerOnOff OnOff=%d \n", OnOff));

	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	if(OnOff)
	{
 		NvOdmGpioSetState(hTouch->hGpio, hTouch->hPinPower, NvOdmGpioPinActiveState_High);
 		
 		msleep(50);
		//Force reset, for some hexing touchsceeen can not boot up
		NvOdmGpioSetState(hTouch->hGpio,
	                	hTouch->hPinReset,
	                	NvOdmGpioPinActiveState_Low);
		msleep(5);
		NvOdmGpioSetState(hTouch->hGpio,
	               		hTouch->hPinReset,
	                	NvOdmGpioPinActiveState_High);
		msleep(60);

		NvOdmGpioInterruptMask(hTouch->hGpioIntr,NV_FALSE);		

	}
	else
	{
		NvOdmGpioInterruptMask(hTouch->hGpioIntr,NV_TRUE);
		NvOdmGpioSetState(hTouch->hGpio, hTouch->hPinPower, NvOdmGpioPinActiveState_Low);
	}
	#endif

	return NV_TRUE;
}

NvBool AT168_Open (NvOdmTouchDeviceHandle* hDevice)
{
	AT168_TouchDevice* hTouch;
	NvU32 i;
	NvU32 found = 0;
	NvU32 I2cInstance = 0;

	NvU32 GpioPort[3] = {0};
	NvU32 GpioPin[3] = {0};
	int GpioNum = 0;

	AT168_PRINTF(("===***NvOdm Touch: AT168_Open***===\n"));

	const NvOdmPeripheralConnectivity *pConnectivity = NULL;

	hTouch = NvOdmOsAlloc(sizeof(AT168_TouchDevice));
	if (!hTouch) return NV_FALSE;

	NvOdmOsMemset(hTouch, 0, sizeof(AT168_TouchDevice));
	/* set function pointers */
	InitOdmTouch(&hTouch->OdmTouch);
	pConnectivity = NvOdmPeripheralGetGuid(AT168_TOUCH_DEVICE_GUID);
	if (!pConnectivity)
	{
		NvOsDebugPrintf("NvOdm Touch : pConnectivity is NULL Error \n");
		goto fail;
	}
	if (pConnectivity->Class != NvOdmPeripheralClass_HCI)
	{
		NvOsDebugPrintf("NvOdm Touch : didn't find any periperal in discovery query for touch device Error \n");
		goto fail;
	}
	for (i = 0; i < pConnectivity->NumAddress; i++)
	{
		switch (pConnectivity->AddressList[i].Interface)
		{
			case NvOdmIoModule_I2c:
				hTouch->DeviceAddr = (pConnectivity->AddressList[i].Address << 1);
				I2cInstance = pConnectivity->AddressList[i].Instance;
				found |= 1;
				break;
			case NvOdmIoModule_Gpio:
				GpioPort[GpioNum] = pConnectivity->AddressList[i].Instance;
				GpioPin[GpioNum++] = pConnectivity->AddressList[i].Address;
				found |= 2;
				break;
			case NvOdmIoModule_Vdd:
				hTouch->VddId = pConnectivity->AddressList[i].Address;
				found |= 4;
				break;
			default:
				break;
		}
	}
	if ((found & 3) != 3)
	{
		NvOsDebugPrintf("NvOdm Touch : peripheral connectivity problem \n");
		goto fail;
	}

	hTouch->hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, I2cInstance);
	if (!hTouch->hOdmI2c)
	{
		NvOsDebugPrintf("NvOdm Touch : NvOdmI2cOpen Error \n");
		goto fail;
	}
	
	hTouch->hGpio = NvOdmGpioOpen();

	if (!hTouch->hGpio)
	{
		NvOsDebugPrintf("NvOdm Touch : NvOdmGpioOpen Error \n");
		goto fail;
	}
	/**********************************************************/
	//note : 
	// Acquiring Pin Handles for all the two Gpio Pins
	// First entry is Reset (Num 0)
	// Second entry should be Interrupt (NUm 1)
	
	//reset
	hTouch->hPinReset = NvOdmGpioAcquirePinHandle(hTouch->hGpio, GpioPort[0], GpioPin[0]);
	if (!hTouch->hPinReset)
	{
		NvOsDebugPrintf("NvOdm Touch : Couldn't get GPIO hPinReset \n");
		goto fail;
	}
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_High);

	NvOdmGpioConfig(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinMode_Output);
	                
	#if 1
	//Force reset, for some hexing touchsceeen can not boot up
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_Low);
	msleep(5);
	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinReset,
	                NvOdmGpioPinActiveState_High);
	msleep(60);
	#endif

	//int
	hTouch->hPinInterrupt = NvOdmGpioAcquirePinHandle(hTouch->hGpio, GpioPort[1], GpioPin[1]);
	if (!hTouch->hPinInterrupt) {
		NvOsDebugPrintf("NvOdm Touch : Couldn't get GPIO hPinInterrupt \n");
		goto fail;
	}
	NvOdmGpioConfig(hTouch->hGpio, hTouch->hPinInterrupt, NvOdmGpioPinMode_InputData);

#if defined(CONFIG_7373C_V20)
	hTouch->hPinPower = NvOdmGpioAcquirePinHandle(hTouch->hGpio, GpioPort[2], GpioPin[2]);
	if (!hTouch->hPinPower) {
		NvOsDebugPrintf("NvOdm Touch : Couldn't get GPIO hPinPower \n");
		//goto fail;
	}
	NvOdmGpioConfig(hTouch->hGpio, hTouch->hPinPower, NvOdmGpioPinMode_Output);

	NvOdmGpioSetState(hTouch->hGpio,
	                hTouch->hPinPower,
	                NvOdmGpioPinActiveState_High);
#endif
	/**********************************************************/
	/* set default capabilities */
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
	/* set default I2C speed */
	hTouch->I2cClockSpeedKHz = AT168_I2C_SPEED_KHZ;
	/**********************************************************/
	#if Calibration_in_Boot_enable
	//set the SPECOP reg
	if (!AT168_WRITE(hTouch, AT168_SPECOP, AT168_SPECOP_CALIBRATION_VALUE))//0x03
	        goto fail;
	#endif	
	/**********************************************************/
	NvU8 InitData[8] = {0};
	if(!AT168_READ(hTouch, AT168_XMAX_LO, InitData, (AT168_VERSION_PROTOCOL - AT168_XMAX_LO + 1)))
	{
		//when can not read , xMax and yMax default value is 1024 X 600 .
		AT168_Capabilities.XMinPosition = 0; //AT168_MIN_X;
		AT168_Capabilities.YMinPosition = 0; //AT168_MIN_Y;
		AT168_Capabilities.XMaxPosition = 1024; //AT168_MAX_X;
		AT168_Capabilities.YMaxPosition = 600; //AT168_MAX_Y;
		NvOsDebugPrintf("NvOdmTouch_at168:  AT168_Open AT168_READ InitData fail .\n");
		//return NV_FALSE;
	}else{
		#if 0
		int j = 0;
		do
		{
			NvOsDebugPrintf("NvOdmTouch_at168: InitData[%d] = 0x%x---\n", j, InitData[j]);
			j++;

		}while(j < 8);
		#endif
	
		//Set the Max and Min position
		AT168_Capabilities.XMinPosition = 0; //AT168_MIN_X;
		AT168_Capabilities.YMinPosition = 0; //AT168_MIN_Y;
		AT168_Capabilities.XMaxPosition = ((InitData[1] << 8) | (InitData[0])); //AT168_MAX_X;
		AT168_Capabilities.YMaxPosition = ((InitData[3] << 8) | (InitData[2])); //AT168_MAX_Y;

		if((0 == AT168_Capabilities.XMaxPosition) 
				|| (0 == AT168_Capabilities.XMaxPosition)
				|| (AT168_Capabilities.XMaxPosition >= 61440) //61440 means 0xf000
				|| (AT168_Capabilities.YMaxPosition >= 61440)){
			NvOsDebugPrintf("=== xMax yMax from touchscreen FW is %d %d, now set default ===\n", AT168_Capabilities.XMaxPosition, AT168_Capabilities.YMaxPosition);	
			AT168_Capabilities.XMaxPosition = 1024;
			AT168_Capabilities.YMaxPosition = 600;
		}
	}
	//Set the Version
	AT168_Capabilities.Version = ((InitData[4] << 24) | (InitData[5] << 16) | (InitData[6] << 8) | (InitData[7]) );

	AT168_Capabilities.CalibrationData = 0;
	NvOsDebugPrintf("NvOdmTouch_at168: now FW xMAX is %d   yMAx is %d Version is %x.\n", AT168_Capabilities.XMaxPosition, AT168_Capabilities.YMaxPosition, AT168_Capabilities.Version);
	/* change the touchscreen capabilities */
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
	/**********************************************************/
	*hDevice = &hTouch->OdmTouch;
	
	AT168_PRINTF(("===NvOdmTouch_at168: AT168_Open success===\n"));	
	return NV_TRUE;

 fail:
	AT168_Close(&hTouch->OdmTouch);
	return NV_FALSE;
}


NvBool AT168_EnableInterrupt (NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hIntSema)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	if (NvOdmGpioInterruptRegister(hTouch->hGpio, &hTouch->hGpioIntr,
        hTouch->hPinInterrupt, NvOdmGpioPinMode_InputInterruptLow, AT168_GpioIsr,
        (void*)hTouch, AT168_DEBOUNCE_TIME_MS) == NV_FALSE)
    	{
        	NvOsDebugPrintf("===Nvodm Touch:AT168_EnableInterrupt NvOdmGpioInterruptRegister fail!=== \n");
		return NV_FALSE;
    	}
	hTouch->hIntSema = hIntSema;
	return NV_TRUE;
}


NvBool AT168_HandleInterrupt(NvOdmTouchDeviceHandle hDevice)
{
	NvU32 PinStateValue;
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	
	NvOdmGpioGetState( hTouch->hGpio, hTouch->hPinInterrupt, &PinStateValue);
	if (!PinStateValue) {
		return NV_FALSE;
	}
	else {
		NvOdmGpioInterruptDone(hTouch->hGpioIntr);
	}
	return NV_TRUE;

}


NvBool AT168_PowerControl (NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode)
{
    switch (mode) 
	{
		case NvOdmTouch_PowerMode_0:
		case NvOdmTouch_PowerMode_1:
		case NvOdmTouch_PowerMode_2:
		case NvOdmTouch_PowerMode_3:
			break;
		default:
			break;
	}
	return NV_TRUE;
}


void AT168_Close (NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;

	if (!hTouch) return;

	if (hTouch->hGpio)
	{
		if (hTouch->hPinInterrupt)
		{
			if (hTouch->hGpioIntr)
			NvOdmGpioInterruptUnregister(hTouch->hGpio, hTouch->hPinInterrupt, hTouch->hGpioIntr);
			NvOdmGpioReleasePinHandle(hTouch->hGpio, hTouch->hPinInterrupt);
		}
		NvOdmGpioClose(hTouch->hGpio);
	}

	if (hTouch->hOdmI2c)
		NvOdmI2cClose(hTouch->hOdmI2c);

	NvOdmOsFree(hTouch);
}

