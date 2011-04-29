/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor : LiuZheng <xmlz@malata.com>
*/

#ifndef __SWITCH_DOCK_INCLUDED__
#define __SWITCH_DOCK_INCLUDED__

#define DOCK_SWITCH_NAME		"dock"

struct dock_switch_platform_data {
	/**
	 * Desktop dock detect gpio number, if you do not have
	 * an desktop dock, leave it 0.
	 */
	int gpio_desktop;				
	
	/**
	 * Desktop gpio active low.
	 */
	int gpio_desktop_active_low;

	/**
	 * Car dock detect gpio number, if you do not have an
	 * "car dock", leave it 0.
	 */
	int gpio_car;					
	
	/**
	 * Car dock gpio active low.
	 */
	int gpio_car_active_low;		
};


#endif /// __SWITCH_DOCK_INCLUDED__
