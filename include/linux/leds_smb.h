/*
 * Copyright (C) 2010 Malata, Corp. All right reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
 */

#ifndef __LEDS_SMB_H_INCLUDED__
#define __LEDS_SMB_H_INCLUDED__

#include <linux/device.h>
#include <linux/leds.h>

struct smb_led_device;

#define COLOR_WHITE				0x00FFFFFF
#define COLOR_BLACK				0x00000000

#define LED_FLASH_NONE			0
#define LED_FLASH_TIMED			1
#define LED_FLASH_HARDWARE		2

#define COLOR_RED				0x00FF0000
#define COLOR_GREEN				0x0000FF00
#define COLOR_BLUE				0x000000FF

#define R(color)				(color & COLOR_RED)
#define G(color)			(color & COLOR_GREEN)
#define B(color)				(color & COLOR_BLUE)

#ifndef	MAX
#define MAX(a, b)				((a > b) ? a : b)
#endif

#define MAIN_COLOR(color) 		MAX(R(color), MAX(G(color), B(color)))

#define COLOR_TO_INTENSITY(x)	\
	((( 77 * ((x >> 16) & 0x00ff)) + \
	  (150 * ((x >>  8) & 0x00ff)) + \
	  ( 29 * ((x >>  0) & 0x00ff))) >> 8)

#define INTENSITY_TO_COLOR(x)

struct smb_led_device {
	struct led_classdev led_cdev;
	char *name;
	int (*set_color)(struct smb_led_device *, unsigned int);
	int (*get_color)(struct smb_led_device *, unsigned int *);
	int (*set_flash_mode)(struct smb_led_device *, unsigned int);
	int (*get_flash_mode)(struct smb_led_device *, unsigned int *);
	int (*set_flash_on)(struct smb_led_device *, unsigned int);
	int (*get_flash_on)(struct smb_led_device *, unsigned int *);
	int (*set_flash_off)(struct smb_led_device *, unsigned int);
	int (*get_flash_off)(struct smb_led_device *, unsigned int *);
};

static inline unsigned int major_color(unsigned int color) 
{
	unsigned int mc, r, g, b;
	if (color == COLOR_BLACK) return COLOR_BLACK;
	r = R(color); g = G(color); b = B(color);
	mc = MAX(r, MAX(g, b));
	if (mc == r) return COLOR_RED;
	else if (mc == g) return COLOR_GREEN;
	else if (mc == b) return COLOR_BLUE;
	else return COLOR_BLACK;
}

extern int smb_led_device_register(struct device *parent, struct smb_led_device *device);
extern void smb_led_device_unregister(struct smb_led_device *device);

#endif
