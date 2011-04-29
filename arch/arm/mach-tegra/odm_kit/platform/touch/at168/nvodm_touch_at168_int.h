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
 
#ifndef AT168_REG_HEADER
#define AT168_REG_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

//*********************************************************************************//
//#define __AT168_DEBUG
#define __AT168_ERROR
#define Calibration_in_Boot_enable	0

#ifdef __AT168_DEBUG
#define AT168_PRINTF(x)	NvOsDebugPrintf x;
#else
#define AT168_PRINTF(x)
#endif

#ifdef __AT168_ERROR
#define AT168_ERROR(x)	NvOsDebugPrintf x;
#else
#define AT168_ERROR(x)
#endif



/* AT168 I2C registers */
#define AT168_TOUCH_NUM			0x00
#define AT168_TOUCH_OLD_NUM			0x01
#define AT168_POS_X0_LO				0x02
#define AT168_POS_X0_HI					0x03
#define AT168_POS_Y0_LO				0x04 
#define AT168_POS_Y0_HI					0x05
#define AT168_POS_X1_LO				0x06 
#define AT168_POS_X1_HI					0x07 
#define AT168_POS_Y1_LO				0x08 
#define AT168_POS_Y1_HI					0x09 
#define AT168_X1_W					0x0a
#define AT168_Y1_W						0x0b
#define AT168_X2_W					0x0c
#define AT168_Y2_W						0x0d
#define AT168_X1_Z					0x0e
#define AT168_Y1_Z						0x0f
#define AT168_X2_Z					0x10
#define AT168_Y2_Z						0x11
#define AT168_POS_PRESSURE_LO		0x12
#define AT168_POS_PRESSURE_HI			0x13
#define AT168_POWER_MODE			0x14
#define AT168_INIT_MODE					0x15

#define AT168_XMAX_LO				0x1a
#define AT168_XMAX_HI					0x1b
#define AT168_YMAX_LO				0x1c
#define AT168_YMAX_HI					0x1d
#define AT168_VERSION_TS_SUPPLIER	0x1e
#define AT168_VERSION_FW				0x1f
#define AT168_VERSION_BOOTLOADER	0x20
#define AT168_VERSION_PROTOCOL			0x21

#define AT168_SINTEK_BASELINE_X1_VALUE				0x7d  //125
#define AT168_SINTEK_BASELINE_X2_VALUE				0x3d  //61
#define AT168_SINTEK_BASELINE_Y_VALUE				0x4d  //77

#define AT168_SINTEK_CALIBRATERESULT_X1_VALUE				0x7d  //125
#define AT168_SINTEK_CALIBRATERESULT_X2_VALUE				0x3d  //61
#define AT168_SINTEK_CALIBRATERESULT_Y_VALUE				0x4d  //77

#define AT168_CANDO_BASELINE_COMMAND				0xc7
#define AT168_CANDO_CALIBRATERESULT_COMMAND		0xc8

#define AT168_SPECOP				0x37

#define AT168_INTERNAL_ENABLE				0xc2 //194

/* AT168 registers Init value*/
#define AT168_POWER_MODE_VALUE	0xa4
#define AT168_INIT_MODE_VALUE		0x0a
#define AT168_SPECOP_DEFAULT_VALUE		0x00
#define AT168_SPECOP_CALIBRATION_VALUE		0x03

#define AT168_INTERNAL_ENABLE_VALUE		0x01
#define AT168_INTERNAL_DISABLE_VALUE		0x00

//*********************************************************************************//
#if defined(__cplusplus)
}
#endif


#endif //AT168_REG_HEADER


