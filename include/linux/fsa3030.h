/*
 * Copyright (C) 2016 HTC Corporation.
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
 */

#ifndef __FSA3030_H__
#define __FSA3030_H__

enum fsa3030_switch_type {
	FSA3030_SWITCH_TYPE_USB,
	FSA3030_SWITCH_TYPE_AUDIO,
	FSA3030_SWITCH_TYPE_MHL,

	FSA3030_SWITCH_TYPE_NUM,
};

extern int fsa3030_switch_set(int handle, enum fsa3030_switch_type type, bool enable);
extern int fsa3030_switch_register(enum fsa3030_switch_type type);
extern int fsa3030_switch_unregister(enum fsa3030_switch_type type);
#endif 
