/*
 * File        : drivers/input/misc/lis35de_accel.h
 * Description : Drviver for lis35de g-sesor
 * By          : LiuZheng xmlz@malata.com 2010/04/23
 */

#ifndef __LIS35DE_ACCEL_PREV_H_INCLUDED__
#define __LIS35DE_ACCEL_PREV_H_INCLUDED__

/*
 * If CTRL1_FS set to 0, 
 * lis35de typically masurement range is set +2.3~-2.3, precision = 4.6g/2^8 = 18 mg/lsb
 *
 * If CTRL1_FS set to 0,
 * lis35de typically msaurement range is set +9.2~-9.2, precesion = 18.4g/256 = 72mg/lsb
 */
#define LIS35DE_PRECISION_MG				18

/*
 * When the object is static, the g value on x,y,z cannot less than 577 at one time.
 */
#define LIS35DE_THRESHOLD_INT_FALLING		300

/*
 * TODO: threshold of click
 */
#define LIS35DE_THRESHOLD_INT_CLICK			

/* Hardware registers */
#define LIS35DE_REG_CTRL_1					(0x20)
#define LIS35DE_REG_CTRL_2					(0x21)
#define LIS35DE_REG_CTRL_3					(0x22)
#define LIS35DE_REG_HP_FILTER_RESET			(0x23)
#define LIS35DE_REG_STATUS					(0x27)
#define LIS35DE_REG_OUTX					(0x29)
#define LIS35DE_REG_OUTY					(0x2B)
#define LIS35DE_REG_OUTZ					(0x2D)
#define LIS35DE_REG_FF_WU_CFG_1				(0x30)
#define LIS35DE_REG_FF_WU_SRC_1				(0x31)
#define LIS35DE_REG_FF_WU_THS_1				(0x32)
#define LIS35DE_REG_FF_WU_DURATION_1		(0x33)
#define LIS35DE_REG_FF_WU_CFG_2				(0x34)
#define LIS35DE_REG_FF_WU_SRC_2				(0x35)
#define LIS35DE_REG_FF_WU_THS_2				(0x36)
#define LIS35DE_REG_FF_WU_DUTATION_2		(0x37)
#define LIS35DE_REG_CLICK_CFG				(0x38)
#define LIS35DE_REG_CLICK_SRC				(0x39)
#define LIS35DE_REG_CLICK_THSY_X			(0x3B)
#define LIS35DE_REG_CLICK_THSZ				(0x3C)
#define LIS35DE_REG_CLICK_TIMELIMIT			(0x3D)
#define LIS35DE_REG_CLICK_LATENCY			(0x3E)
#define LIS35DE_REG_CLICK_WINDOW			(0x3F)

/*
 * CTRL_REG1:
 *
 * DR : Date rate selection. 0: 100Hz; 1: 400Hz; default: 1
 * PD : Power Down Control. 0: power down mode; 1: active mode; default: 1
 * FS : Full Scale selection. TODO: ??
 * ZEN : Z axis enable, default value 1
 * YEN : Y axis enable.
 * XEN : X axis enable.
 *
 * The bitmap is:
 *
 * |------|------|------|------|------|------|------|------|
 * |  DR     PD     FS      0      0    ZEN    YEN    XEN  |
 * |------|------|------|------|------|------|------|------|
 */
#define LIS35DE_REG_CTRL_1_MS_DR			(0x80)
#define LIS35DE_REG_CTRL_1_MS_PD			(0x40)
#define LIS35DE_REG_CTRL_1_MS_FS			(0x20)
#define LIS35DE_REG_CTRL_1_MS_ZEN			(0x04)
#define LIS35DE_REG_CTRL_1_MS_YEN			(0x02)
#define LIS35DE_REG_CTRL_1_MS_XEN			(0x01)

/*
 * CTRL_REG2:
 *
 * SIM  : SPI Serial Interface Mode selection. 
 *        0: 4-wire; 
 *        1: 3-wire
 *
 * BOOT : Reboot memory content. 
 *        0: normal mode; 
 *        1: reboot memory content
 *
 * FDS  : Filtered Data Selection.
 *        0: interla filter bypassed; 
 *        1: data from internal filter sent to output register.
 * 
 * HP_FF_WU2 : High Pass filter enabled for FreeFall/WakeUp
 * HP_FF_WU1 : High Pass filter enabled for FreeFall/WakeUp
 *
 * HP_COEFF2 : These 2 bit are used to configure high-pass filter cut-off frequency ft
 * HP_COEFF1 : 
 * 
 */
#define LIS35DE_REG_CTRL_2_MS_SIM			(0x80)
#define LIS35DE_REG_CTRL_2_MS_BOOT			(0x40)
#define LIS35DE_REG_CTRL_2_MS_FDS			(0x10)
#define LIS35DE_REG_CTRL_2_MS_HP_FF_WU2		(0x08)
#define LIS35DE_REG_CTRL_2_MS_HP_FF_WU1		(0x04)
#define LIS35DE_REG_CTRL_2_MS_HP_COEFF2		(0x02)
#define LIS35DE_REG_CTRL_2_MS_HP_COEFF1		(0x01)

/*
 * CTRL_REG3 : Interrupt CTRL register
 * 
 * IHL : Interrupt active high or low. default 0.
 *       1: active high
 *       0: active low
 *
 * PP_OD : Push-pull/Open Drain selection on interrupt pad. default 0.
 *         0: push-pull
 *         1: open-drain
 *
 * I1(2)CFG:
 * 		000 : GND
 * 		001 : FF_WU_1
 * 		010 : FF_WU_2
 * 		011 : FF_WU_1 OR FF_WU_2
 * 		100 : Data Ready
 * 		111 : Click Interrupt
 */
#define LIS35DE_REG_CTRL_3_MS_IHL
#define LIS35DE_REG_CTRL_3_MS_PP_OD
#define LIS35DE_REG_CTRL_3_MS_I2CFG2
#define LIS35DE_REG_CTRL_3_MS_I2CFG1
#define LIS35DE_REG_CTRL_3_MS_I2CFG0
#define LIS35DE_REG_CTRL_3_MS_I1CFG2
#define LIS35DE_REG_CTRL_3_MS_I1CFG0

/*
 * STATUS_REG
 * 
 * ZXYOR : X, Y and Z axis Data Overrun
 *         0: no overrun occured.
 *         1: new data has over written the previos one before it was read.
 * 
 * ZOR : 0: no overrun
 *       1: a new data overwritten
 * 
 * YOR : 0: no overrun
 *       1: a new data overwritten
 *
 * XOR : 0: no overrun
 *       1: a new data overwriten
 *
 * ZYXDA : X, Y and Z axis new Data Avalable.
 *         0: not yet avaliable
 *         1: avaliable
 *
 * ZDA : 0: not yet avaliable
 *       1: a new data avaliable
 *
 * YDA : 0: not yet avaliable
 *       1: a new data avalible
 * 
 * XDA : 0: not yet avaliable
 *       1: a new data avalible
 */
#define LIS35DE_REG_STATUS_MS_ZXYOR			(0x80)
#define LIS35DE_REG_STATUS_MS_ZOR			(0x40)
#define LIS35DE_REG_STATUS_MS_YOR			(0x20)
#define LIS35DE_REG_STATUS_MS_XOR			(0x10)
#define LIS35DE_REG_STATUS_MS_ZYXDATA		(0x08)
#define LIS35DE_REG_STATUS_MS_ZDA			(0x04)
#define LIS35DE_REG_STATUS_MS_YDA			(0x02)
#define LIS35DE_REG_STATUS_MS_XDA			(0x01)

/*
 * FF_WU_CFG_1:
 *
 * AOI : And/Or combination of interrupt events
 *       0: OR
 *       1: AND
 *
 * LIR : Latch Interrupt request into FF_WU_SRC reg with the FF_WU_SRC reg cleared by reading FF_WU_SRC_1 reg.
 *       0: not latch
 *       1: latch
 *
 * ZHIE : Enable interrupt Z high
 * ZLIE : Enable interrupt Z low
 * YHIE : Enable interrupt Y high
 * YHIE : Enable interrupt Y low
 * XHIE : Enable interrupt X high
 * XLIE : Enable interrupt X low
 */
#define LIS35DE_REG_FF_WU_CFG_1_MS_AOI		(0x80)
#define LIS35DE_REG_FF_WU_CFG_1_MS_LIR		(0x40)
#define LIS35DE_REG_FF_WU_CFG_1_MS_ZHIE		(0x20)
#define LIS35DE_REG_FF_WU_CFG_1_MS_ZLIE		(0x10)
#define LIS35DE_REG_FF_WU_CFG_1_MS_YHIE		(0x08)
#define LIS35DE_REG_FF_WU_CFG_1_MS_YLIE		(0x04)
#define LIS35DE_REG_FF_WU_CFG_1_MS_XHIE		(0x02)
#define LIS35DE_REG_FF_WU_CFG_1_MS_XLIE		(0x01)

/*
 * FF_WU_SRC_1
 *
 * IA : Interrupt Active.
 *      0: no interrupt generate
 *      1: one or more generate
 *
 **/
#define LIS35DE_REG_FF_WU_SRC_1_MS_IA				
#define LIS35DE_REG_FF_WU_SRC_1_MS_ZH
#define LIS35DE_REG_FF_WU_SRC_1_MS_ZL
#define LIS35DE_REG_FF_WU_SRC_1_MS_YH
#define LIS35DE_REG_FF_WU_SRC_1_MS_YL
#define LIS35DE_REG_FF_WU_SRC_1_MS_XH
#define LIS35DE_REG_FF_WU_SRC_1_MS_XL

#endif
