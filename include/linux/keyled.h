/*
 * include/linux/keyreset.h - platform data structure for resetkeys driver
 *
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *KEY_LED_V1
 */

#ifndef _LINUX_KEYLED_H
#define _LINUX_KEYLED_H

#define KEYLED_NAME "keyled"
#define PLATFORM_DEVID_AUTO	(-2)

struct keyled_platform_data {
	uint32_t key_down_delay;
	uint32_t *keys_up;
	uint32_t *keys_down; 
};

#endif 
