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

#ifndef INCLUDED_NVODM_TOUCH_AT168_H
#define INCLUDED_NVODM_TOUCH_AT168_H

#include "../nvodm_touch_int.h"
#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool AT168_Open( NvOdmTouchDeviceHandle *hDevice);

void AT168_GetCapabilities(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities);

void AT168_SetCalibration(NvOdmTouchDeviceHandle hDevice);

NvBool AT168_ReadCoordinate( NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *coord);

NvBool AT168_EnableInterrupt(NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hInterruptSemaphore);

NvBool AT168_HandleInterrupt(NvOdmTouchDeviceHandle hDevice);

NvBool AT168_GetSampleRate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate);

NvBool AT168_SetSampleRate(NvOdmTouchDeviceHandle hDevice, NvU32 rate);

NvBool AT168_PowerControl(NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode);

NvBool AT168_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer);

NvBool AT168_PowerOnOff(NvOdmTouchDeviceHandle hDevice, NvBool OnOff);

void AT168_Close( NvOdmTouchDeviceHandle hDevice);

NvBool AT168_BurnSintexBootloader(NvOdmTouchDeviceHandle hDevice);

NvBool AT168_BurnCandoBootloader(NvOdmTouchDeviceHandle hDevice);

NvBool AT168_BurnBootloader(NvOdmTouchDeviceHandle hDevice);

void AT168_SetBaseline(NvOdmTouchDeviceHandle hDevice);

void AT168_SetCalibrateResult(NvOdmTouchDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif


#endif // INCLUDED_NVODM_TOUCH_AT168_H
