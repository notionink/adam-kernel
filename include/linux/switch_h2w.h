/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng<xmlz@malata.com>
 */

#ifndef __SWITCH_H2W_H_INCLUDED__
#define __SWITCH_H2W_H_INCLUDED__

#define H2W_SWITCH_DEV_NAME		"h2w"

struct switch_h2w_platform_data {
	int 	hp_det_pin;
	int		hp_det_port;
	int		hp_det_active_low;

	int 	have_dock_hp;
	int 	dock_hp_det_pin;
	int 	dock_hp_det_port;
	int		dock_hp_det_active_low;
};

#endif
