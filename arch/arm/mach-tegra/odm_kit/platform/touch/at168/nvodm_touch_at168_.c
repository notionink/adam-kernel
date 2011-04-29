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

#define AT168_I2C_SPEED_KHZ                          200
#define AT168_I2C_TIMEOUT                            500
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
};

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
	Dev->OutputDebugMessage = NV_FALSE;
}

void AT168_SetCalibration(NvOdmTouchDeviceHandle hDevice)
{
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
		
	//set the SPECOP reg and the touchscreen will calibration by itself;
	AT168_WRITE(hTouch, AT168_SPECOP, AT168_SPECOP_CALIBRATION_VALUE);

	NvOsDebugPrintf("AT168_SetCalibration OK .\n");
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
	AT168_PRINTF(("NvOdm Touch: AT168_PowerOnOff OnOff=%d", OnOff));
	AT168_TouchDevice* hTouch = (AT168_TouchDevice*)hDevice;
	if(!OnOff)
	{
		NvOdmGpioInterruptMask(hTouch->hGpioIntr,NV_TRUE);
	}
	else
	{
		NvOdmGpioInterruptMask(hTouch->hGpioIntr,NV_FALSE);
		
	}
	return NV_TRUE;
}

NvBool AT168_Open (NvOdmTouchDeviceHandle* hDevice)
{
	AT168_TouchDevice* hTouch;
	NvU32 i;
	NvU32 found = 0;
	NvU32 I2cInstance = 0;

	NvU32 GpioPort[2] = {0};
	NvU32 GpioPin[2] = {0};
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
		NvOsDebugPrintf("NvOdmTouch_at168:  AT168_Open AT168_READ InitData fail .\n");
		return NV_FALSE;
	}

	#if 0
	int j = 0;
	do
	{
		AT168_PRINTF(("NvOdmTouch_at168: InitData[%d] = 0x%x---\n", j, InitData[j]));
		j++;

	}while(j < 8);
	#endif
	
	//Set the Max and Min position
	AT168_Capabilities.XMinPosition = 0; //AT168_MIN_X;
	AT168_Capabilities.YMinPosition = 0; //AT168_MIN_Y;
	AT168_Capabilities.XMaxPosition = ((InitData[1] << 8) | (InitData[0])); //AT168_MAX_X;
	AT168_Capabilities.YMaxPosition = ((InitData[3] << 8) | (InitData[2])); //AT168_MAX_Y;
	
	//Set the Version
	AT168_Capabilities.Version = ((InitData[4] << 24) | (InitData[5] << 16) | (InitData[6] << 8) | (InitData[7]) );


	#if 1	 //for old version of hexing touchscreen , when hexing FW update, mask them
	if((AT168_Capabilities.XMaxPosition != 4096) || (AT168_Capabilities.YMaxPosition != 4096))
	{
		AT168_PRINTF(("NvOdmTouch_at168: It is HeXing touchscreen , old F/W , now set 1024 X 600 .\n"));
		AT168_Capabilities.XMaxPosition = 1024;
		AT168_Capabilities.YMaxPosition = 600;
	}
	if(0x00 == (InitData[4]) )
	{
		AT168_PRINTF(("NvOdmTouch_at168: It is HeXing touchscreen , old F/W , now set version .\n"));
		AT168_Capabilities.Version = 0x02010112;
	}
	#endif
	
	AT168_PRINTF(("NvOdmTouch_at168: now xMAX is %d   yMAx is %d.\n", AT168_Capabilities.XMaxPosition, AT168_Capabilities.YMaxPosition));
	/* change the touchscreen capabilities */
	NvOdmOsMemcpy(&hTouch->Caps, &AT168_Capabilities, sizeof(NvOdmTouchCapabilities));
	/**********************************************************/
	*hDevice = &hTouch->OdmTouch;
	
	NvOsDebugPrintf("===NvOdmTouch_at168: AT168_Open success===\n");	
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

