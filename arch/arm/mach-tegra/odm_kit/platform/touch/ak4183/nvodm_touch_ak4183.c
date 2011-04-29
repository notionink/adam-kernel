				/*
 ************************************************************************
 * File name   :
 * Description :
 * Log         :
 ************************************************************************
 */

#include "nvos.h"
#include "nvodm_services.h"
#include "nvodm_query.h"
#include "nvodm_query_discovery.h"

#include "../nvodm_touch_int.h"
#include "nvodm_touch.h"

#include "nvodm_touch_ak4183.h"
#include "nvodm_touch_ak4183_int.h"

#define AK4183_PRESSURE_LIMIT		(1900)
#define AK4183_COLLECT_NR			(4)
#define TOUCH_VALID_VALUE			(2)
/*
 * fore_refrence
 */
static void InitOdmTouch(NvOdmTouchDevice *Dev);
static void AK4183_GpioIsr(void* args);
static NvBool AK4183_Write_Cmd(AK4183_TouchDevice *pAK4183Touch, NvU8 *buffer, NvU8 NumBytes);
static NvBool AK4183_Read_Dat(AK4183_TouchDevice *pAK4183Touch, NvU16 *buffer);
static NvBool AK4183_ReadXCoord(AK4183_TouchDevice *pAK4183Touch, NvU16 *buffer);
static NvBool AK4183_ReadYCoord(AK4183_TouchDevice *pAK4183Touch, NvU16 *buffer);
static NvBool AK4183_ReadZ1Pressure(AK4183_TouchDevice *pAK4183Touch, NvU16 *buffer);
static NvBool AK4183_ReadZ2Pressure(AK4183_TouchDevice *pAK4183Touch, NvU16 *buffer);
static void NvU16BubbleSort(NvU16 *buf, NvU32 num);
static NvBool GetValidCoord(NvU16 *buf, NvU32 num, NvU16 *valid);

void AK4183_Close(NvOdmTouchDeviceHandle hTouch);

/*
 * varibles
 */
NvOdmTouchCapabilities AK4183TouchCapabilities = {
	.IsMultiTouchSupported = NV_FALSE,
	.MaxNumberOfFingerCoordReported = 1,
	.IsRelativeDataSupported = NV_FALSE,
	.MaxNumberOfRelativeCoordReported = 0,
	.MaxNumberOfWidthReported = 0,
	.MaxNumberOfPressureReported = 0,
	.Gesture = NvOdmTouchGesture_Not_Supported,
	.IsWidthSupported = NV_FALSE,
	.IsPressureSupported = NV_FALSE,
	.IsFingersSupported = NV_FALSE,
	.XMinPosition = AK4183_MIN_X,
	.YMinPosition = AK4183_MIN_Y,
	.XMaxPosition =	AK4183_MAX_X,
	.YMaxPosition = AK4183_MAX_Y,
	.Orientation = 0,//NvOdmTouchOrientation_V_FLIP,	//NvOdmTouchOrientation_H_FLIP | 	 ?
	//.Orientation = NvOdmTouchOrientation_H_FLIP | NvOdmTouchOrientation_V_FLIP,
};

#ifdef __AK4183_TEST
static void _for_test( AK4183_TouchDevice *touch) 
{
	//AK4183_ReadXCoord();
	NvU16 x, y, z1, z2;
	while (1) {
		AK4183_ReadXCoord(touch, &x);
		AK4183_ReadYCoord(touch, &y);
		AK4183_ReadZ1Pressure(touch, &z1);
		AK4183_ReadZ2Pressure(touch, &z2);

		NvOsDebugPrintf("----------------------------------\n");
		NvOsDebugPrintf("x=%d\ny=%d\nz1=%d\nz2=%d\n", x, y, z1, z2);
		NvOdmOsSleepMS(200);
	}
}
#endif	/// __AK4183_TEST
# if 0
static void _for_test2() 
{
	NvOdmServicesGpioHandle hGpio;
	NvOdmGpioPinHandle hPin;
	hGpio = NvOdmGpioOpen();
	hPin = NvOdmAcquirePinHandle(hGpio, 'u'-'a', 5);
	NvOdmGpioConfig(hGpio, hPin, NvOdmGpioMode_InputData);
	NvOdmGpioSEt

}
#endif 
NvBool AK4183_Open(NvOdmTouchDeviceHandle *hDevice)
{
	int i;
	AK4183_TouchDevice *pAK4183Touch;
	NvOdmPeripheralConnectivity *pConnectivity = NULL;
	NvU32 I2cInstance = 0;
	NvU32 GpioPort = 0;
	NvU32 GpioPin = 0;
	NvU32 found = 0;
	NvU8 cmd;

	NvOsDebugPrintf("==================================================\n");
	NvOsDebugPrintf("NvOdm Touch: AK4183_Open\n");
	NvOsDebugPrintf("===================================================\n");

	pAK4183Touch = NvOsAlloc(sizeof(AK4183_TouchDevice));
	if (!pAK4183Touch) {
		NvOsDebugPrintf("AK4183_Open: NvOsAlloc() daminic alloc memory failed...\n");
		return NV_FALSE;
	}
	NvOsMemset(pAK4183Touch, 0, sizeof(AK4183_TouchDevice));

	InitOdmTouch(&pAK4183Touch->OdmTouch);

	pConnectivity = NvOdmPeripheralGetGuid(AK4183_TOUCH_DEVICE_GUID);
	if (!pConnectivity || pConnectivity->Class != NvOdmPeripheralClass_HCI ) {
		AK4183_PRINTF(("AK4183_Open: peripheral_connectivity problem:(no guid or wrong class)\n"));
		goto fail;
	}

	for (i = 0; i < pConnectivity->NumAddress; i++) {
		switch (pConnectivity->AddressList[i].Interface) {
			case NvOdmIoModule_I2c:
				I2cInstance = pConnectivity->AddressList[i].Instance;
				pAK4183Touch->DeviceAddress = pConnectivity->AddressList[i].Address;
				found |= 1;
				break;
			case NvOdmIoModule_Gpio:
				GpioPort = pConnectivity->AddressList[i].Instance;
				GpioPin = pConnectivity->AddressList[i].Address;
				found |= 2;
				break;
			case NvOdmIoModule_Vdd:
				found |= 4;
				break;
			default:
				break;
		}
	}
	
	if ((found & 3) != 3) {
		NvOsDebugPrintf("AK4183_Touch: peripheral_connectivity problem\n");
		goto fail;
	}

	pAK4183Touch->hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, I2cInstance);
	if (!pAK4183Touch->hOdmI2c) {
		AK4183_PRINTF(("NvOdm Touch: I2c Open failed\n"));
		goto fail;
	}
	pAK4183Touch->I2cClockSpeedKHz = AK4183_I2C_SPEED_KHZ;

	pAK4183Touch->hGpio = NvOdmGpioOpen();
	if (!pAK4183Touch->hGpio) {
		AK4183_PRINTF(("NvOdm Touch: Gpio Open fialed\n"));
		goto fail;
	}
	pAK4183Touch->hPin = NvOdmGpioAcquirePinHandle(pAK4183Touch->hGpio, GpioPort, GpioPin);
	if (!pAK4183Touch->hPin) {
		AK4183_PRINTF(("NvOdmTouch: Gpio acquire pin failed.\n"));
	}
	NvOdmGpioConfig(pAK4183Touch->hGpio, pAK4183Touch->hPin, NvOdmGpioPinMode_InputData);

	NvOdmOsMemcpy(&pAK4183Touch->Caps, &AK4183TouchCapabilities, sizeof(NvOdmTouchCapabilities));

	*hDevice = &pAK4183Touch->OdmTouch;

	cmd = AK4183_CMD_INIT;
	if(NV_FALSE==AK4183_Write_Cmd(*hDevice, &cmd, 1)) goto fail;

#ifdef __AK4183_TEST
	_for_debug(pAK4183Touch);
#endif	// __AK4183_TEST
	return NV_TRUE;
fail:
	AK4183_Close((NvOdmTouchDeviceHandle)pAK4183Touch);
	return false;

}

void AK4183_Close(NvOdmTouchDeviceHandle hTouch) 
{
	AK4183_PRINTF(("%s\n", __func__));
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)(hTouch);
	if (pAK4183Device->hGpioIntr) {
		NvOdmGpioInterruptDone(pAK4183Device->hGpioIntr);
		NvOdmGpioInterruptUnregister(pAK4183Device->hGpio, pAK4183Device->hPin, pAK4183Device->hGpioIntr);
	}
	if (pAK4183Device->hGpio && pAK4183Device->hPin) {
		NvOdmGpioReleasePinHandle(pAK4183Device->hGpio, pAK4183Device->hPin);
		NvOdmGpioClose(pAK4183Device->hGpio);
	}
	if (pAK4183Device->hOdmI2c) {
		NvOdmI2cClose(pAK4183Device->hOdmI2c);
	}
	NvOsFree(pAK4183Device);
}

NvBool AK4183_GetCapabilities(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities *pCapabilities)
{
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)(hDevice);
	//memcpy(pCapabilities, &pAK4183Device->Cpas, sizeof(NvOdmTouchCapabilities));
	NvOdmOsMemcpy(pCapabilities, &pAK4183Device->Caps, sizeof(NvOdmTouchCapabilities));
	return NV_TRUE;
}

NvBool AK4183_ReadCoordinate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *pCoord)
{
	NvU32 i, validNum;
	NvU16 xcoord, ycoord, z1pressure, z2pressure;
	NvU16 xbuf[AK4183_COLLECT_NR], ybuf[AK4183_COLLECT_NR];
	NvU16 z1buf[AK4183_COLLECT_NR], z2buf[AK4183_COLLECT_NR];
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)hDevice;
	NvU32 i2c_snag = 0;
	validNum = 0;
	NvU32 limit;

	for (i = 0; i < AK4183_COLLECT_NR; i++) {
		AK4183_ReadXCoord(pAK4183Device, &xbuf[validNum]);
		AK4183_ReadYCoord(pAK4183Device, &ybuf[validNum]);
		AK4183_ReadZ1Pressure(pAK4183Device, &z1buf[validNum]);
		AK4183_ReadZ2Pressure(pAK4183Device, &z2buf[validNum]);
		
		if ((xbuf[validNum] <= AK4183_MAX_X && xbuf >= AK4183_MIN_X) &&
			(ybuf[validNum] <= AK4183_MAX_Y && ybuf >= AK4183_MIN_Y) &&
			z1buf[validNum] != 0) {
			validNum ++;
		}
			
#if 0
		if ( (AK4183_ReadXCoord(pAK4183Device, &xbuf[validNum]))
			 && (xbuf[validNum] <= AK4183_MAX_X)
			 && (xbuf[validNum] >= AK4183_MIN_X)
			 && (AK4183_ReadYCoord(pAK4183Device, &ybuf[validNum]))
			 && (ybuf[validNum] <= AK4183_MAX_Y)
			 && (ybuf[validNum] >= AK4183_MIN_Y)
			 && (AK4183_ReadZ1Pressure(pAK4183Device, &z1buf[validNum]))
			 && (z1buf[validNum] != 0)
			 && (AK4183_ReadZ2Pressure(pAK4183Device, &z2buf[validNum])) )
		{

			limit = xbuf[validNum]*z2buf[validNum]/z1buf[validNum]-xbuf[validNum];
			NvOsDebugPrintf("limit=%d\n",limit);
			//if (limit < AK4183_PRESSURE_LIMIT)
			//{
				validNum++;
			//}
		}
#endif
	}
	if (validNum < TOUCH_VALID_VALUE) {
		pCoord->additionalInfo.Fingers = 0;
		return NV_TRUE;
	}

	NvU16BubbleSort(xbuf, validNum);
	NvU16BubbleSort(ybuf, validNum);
	if (!GetValidCoord(xbuf, validNum, &xcoord) ||
		!GetValidCoord(ybuf, validNum, &ycoord)) {
		xcoord = 0; ycoord = 0;
		for (i = 0; i < validNum; i++) {
			xcoord += xbuf[i];
			ycoord += ybuf[i];
		}
		xcoord /= validNum;
		ycoord /= validNum;
	}

	pCoord->xcoord = xcoord;
	pCoord->ycoord = ycoord;
	pCoord->additionalInfo.Fingers = 1;
	AK4183_PRINTF(("\nNvOdm Touch: x=%d, y=%d\n", xcoord, ycoord));
	return NV_TRUE;
failed:
	AK4183_PRINTF(("NvOdm Touch: AK4183_ReadCoordinate failed\n"));
	return NV_FALSE;
}

#if 0
NvBool AK4183_ReadCoordinate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *pCoord)
{
	NvBool ret;
	NvU8 cmd;
	NvU16 xcoord, ycoord;
	NvU16 z1, z2;
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)hDevice;
	
	// Read X
	cmd = AK4183_CMD_MEASURE_X;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) goto failed;
	ret = AK4183_Read_Dat(pAK4183Device, &xcoord);
	if (!ret) goto failed;

	// Read Y
	cmd = AK4183_CMD_MEASURE_Y;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) goto failed;
	ret = AK4183_Read_Dat(pAK4183Device, &ycoord);
	if (!ret) goto failed;

	// Read Z1
	cmd = AK4183_CMD_MEASURE_Z1;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) goto failed;
	ret = AK4183_Read_Dat(pAK4183Device, &z1);
	if (!ret) goto failed;

	cmd = AK4183_CMD_MEASURE_Z2;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) goto failed;
	ret = AK4183_Read_Dat(pAK4183Device, &z2);
	if (!ret) goto failed;

	if (z1 && z2) {
		x*z2/z1 - x
	}


	pCoord->xcoord = xcoord;
	pCoord->ycoord = ycoord;
	pCoord->additionalInfo.Fingers = 1;
	pCoord->fingerstate = NvOdmTouchSampleValidFlag;
	NvOsDebugPrintf("NvOdm Touch: ReadCoordinate success\n");
	return NV_TRUE;

failed:
	NvOsDebugPrintf("NvOdm Touch: ReadCoordiante failed:\n");
	return NV_FALSE;
}

#endif

NvBool AK4183_EnableInterrupt(NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hInterruptSemaphore) 
{
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)hDevice;
	if (NvOdmGpioInterruptRegister( pAK4183Device->hGpio,
									&pAK4183Device->hGpioIntr,	// Return the gpio interrupt handle
									pAK4183Device->hPin,
									NvOdmGpioPinMode_InputInterruptLow,
									AK4183_GpioIsr,				// Callback
									(void *)pAK4183Device,
									AK4183_DEBOUNCE_TIME_MS) == NV_FALSE) 
	{
		AK4183_PRINTF(("NvOdm Touch: AK4183_EnableInterrupt failed\n"));
		return NV_FALSE;
	}

	pAK4183Device->hIntrSema = hInterruptSemaphore;
	return NV_TRUE;
}

NvBool AK4183_PowerControl(NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode)
{
	switch (mode) {
		case NvOdmTouch_PowerMode_0:
		case NvOdmTouch_PowerMode_1:
		case NvOdmTouch_PowerMode_2:
		case NvOdmTouch_PowerMode_3:
			break;
		default:
			break;
	}
	AK4183_PRINTF(("NvOdm Touch: AK4183_PowerControl mode = %d", mode));
	return NV_TRUE;
}

NvBool AK4183_PowerOnOff(NvOdmTouchDeviceHandle hDevice, NvBool OnOff) 
{
	AK4183_PRINTF(("NvOdm Touch: AK4183_PowerOnOff OnOff=%d", OnOff));
	return NV_TRUE;
}

NvBool AK4183_HandleInterrupt(NvOdmTouchDeviceHandle hDevice)
{
	NvU32 PinStateValue;
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)hDevice;

	NvOdmGpioGetState( pAK4183Device->hGpio, pAK4183Device->hPin, &PinStateValue);
	if (!PinStateValue) {
		//NvOsDebugPrintf("NvOdm Touch: Gpio state low\n");
		return NV_FALSE;
	}
	else {
		//NvOsDebugPrintf("NvOdm Touch: GPio state high\n");
		NvOdmGpioInterruptDone(pAK4183Device->hGpioIntr);
	}
	return NV_TRUE;
}

NvBool AK4183_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvU32 *pRawCoordBuffer)
{
	NvOsDebugPrintf("NvOdm Touch: AK4183_GetCalibrationData n");
	return NV_FALSE;
}
static void InitOdmTouch(NvOdmTouchDevice *Dev)
{
	Dev->ReadCoordinate 	= AK4183_ReadCoordinate;	//
	Dev->EnableInterrupt 	= AK4183_EnableInterrupt;	//
	Dev->HandleInterrupt 	= AK4183_HandleInterrupt;	//
	Dev->GetSampleRate 		= NULL;						//
	Dev->SetSampleRate 		= NULL;						//
	Dev->PowerControl 		= AK4183_PowerControl;		//
	Dev->PowerOnOff 		= AK4183_PowerOnOff;		//
	Dev->GetCapabilities 	= AK4183_GetCapabilities;	//
	Dev->GetCalibrationData = NULL;						//
	Dev->Close 				= AK4183_Close;				//
}

static NvBool AK4183_Read_Dat(AK4183_TouchDevice *pAK4183Touch, NvU16* buffer)
{
	NvU8 data[2];
	NvU16 value;
	NvOdmI2cTransactionInfo TransactionInfo;
	NvOdmI2cStatus err;

	TransactionInfo.Flags = 0;
	TransactionInfo.Address = pAK4183Touch->DeviceAddress;
	TransactionInfo.NumBytes = 2;
	TransactionInfo.Buf = data;
	err = NvOdmI2cTransaction(pAK4183Touch->hOdmI2c,
								&TransactionInfo,
								1,
								pAK4183Touch->I2cClockSpeedKHz,
								AK4183_I2C_TIMEOUT);
	if (err != NvOdmI2cStatus_Success) {
		NvOsDebugPrintf("NvOdm_Touch: AK4183_Read_Data I2c transaction fatiture = %d\n, address=%d", err, TransactionInfo.Address);
		return NV_FALSE;
	}
	value = (data[0] & 0xff) << 4;
	value |= ((data[1] & 0xff) >> 4);
	value &= 0xfff;
	*buffer = value;
	return NV_TRUE;
}

static NvBool AK4183_Write_Cmd(AK4183_TouchDevice* pAK4183Touch, NvU8 *buffer, NvU8 NumBytes)
{
	NvOdmI2cTransactionInfo TransactionInfo;
	NvOdmI2cStatus err;

	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = pAK4183Touch->DeviceAddress;
	TransactionInfo.NumBytes = NumBytes;
	TransactionInfo.Buf = buffer;
	err = NvOdmI2cTransaction(pAK4183Touch->hOdmI2c,
								&TransactionInfo,
								1,
								pAK4183Touch->I2cClockSpeedKHz,
								AK4183_I2C_TIMEOUT);
	if (err != NvOdmI2cStatus_Success) {
		NvOsDebugPrintf("NvOdmTouch: AK4183_Write_Cmd i2c transaction failture = %d, address=%d\n", err, TransactionInfo.Address);
		return NV_FALSE;
	}
	return NV_TRUE;
}

static void AK4183_GpioIsr(void *args)
{
	//NvOsDebugPrintf("NvOdm Touch: AK4183_GpioIsr\n");
	AK4183_TouchDevice *pAK4183Device = (AK4183_TouchDevice *)args;
	NvOdmOsSemaphoreSignal(pAK4183Device->hIntrSema);
}

static NvBool AK4183_ReadXCoord(AK4183_TouchDevice *pAK4183Device, NvU16 *buffer)
{
	NvBool ret;
	NvU8 cmd = AK4183_CMD_MEASURE_X;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) return NV_FALSE;
	cmd = AK4183_CMD_ACCELERATE_X;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) return NV_FALSE;
	ret = AK4183_Read_Dat(pAK4183Device, buffer);
	if (!ret) return NV_FALSE;
	return NV_TRUE;
}

static NvBool AK4183_ReadYCoord(AK4183_TouchDevice *pAK4183Device, NvU16 *buffer)
{
	NvBool ret;
	NvU8 cmd = AK4183_CMD_MEASURE_Y;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	cmd = AK4183_CMD_ACCELERATE_Y;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) return NV_FALSE;
	ret = AK4183_Read_Dat(pAK4183Device, buffer);
	if (!ret) return NV_FALSE;
	return NV_TRUE;
}
static NvBool AK4183_ReadZ1Pressure(AK4183_TouchDevice *pAK4183Device, NvU16 *buffer)
{
	NvBool ret;
	NvU8 cmd = AK4183_CMD_MEASURE_Z1;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) return NV_FALSE;
	ret = AK4183_Read_Dat(pAK4183Device, buffer);
	if (!ret) return NV_FALSE;
	return NV_TRUE;
}

static NvBool AK4183_ReadZ2Pressure(AK4183_TouchDevice *pAK4183Device, NvU16 *buffer)
{
	NvBool ret;
	NvU8 cmd = AK4183_CMD_MEASURE_Z2;
	ret = AK4183_Write_Cmd(pAK4183Device, &cmd, 1);
	if (!ret) return NV_FALSE;
	ret = AK4183_Read_Dat(pAK4183Device, buffer);
	if (!ret) return NV_FALSE;
	return NV_TRUE;
}

/*
 * Get the max refrence number.
 */
static NvBool GetValidCoord(NvU16 *buf, NvU32 num, NvU16 *valid)
{
	NvU32 i;
	NvU16 bingle = buf[0], cursor;
	NvU32 bingle_nr = 0, cursor_nr = 1;
	NvU32 branch = 0;

	for (i = 0; i < num; i++) {
		if (cursor != buf[i]) {
			if (bingle_nr < cursor_nr) {
				bingle = cursor;
				bingle_nr = cursor_nr;
				branch = 0;
			} else if (bingle_nr == cursor_nr) {
				branch++;
			}
			cursor = buf[i];
			cursor_nr = 1;
		} else {
			cursor_nr++;
		}
	}

	if (bingle_nr < cursor_nr) {
		bingle = cursor;
		bingle_nr = cursor_nr;
		branch = 0;
	} else if (bingle_nr == cursor_nr) {
		branch++;
	}

	if (branch) {
		return NV_FALSE;
	}

	*valid = bingle;
	return NV_TRUE;
}

/*
 * Sort value by bubble
 */
static void NvU16BubbleSort(NvU16 *buf, NvU32 num)
{
	NvU32 i, j;
	NvU16 temp;
	for (i = 0; i < num; i++) {
		for (j = i+1; j < num; j++) {
			if (buf[i] > buf[j]) {
				temp = buf[i];
				buf[i] = buf[j];
				buf[j] = temp;
			}
		}
	}

}



