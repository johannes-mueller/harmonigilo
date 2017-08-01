/*
    Copyright (C) 2016 Johannes Mueller <github@johannes-mueller.org>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef HRM_H
#define HRM_H

#define HRM_URI "http://johannes-mueller.org/oss/lv2/harmonigilo#"

#define CHAN_NUM 6

#define MAXDELAY 1000.0

typedef enum {
	HRM_ENABLED_0 = 0,
 	HRM_DELAY_0 = 1,
	HRM_PITCH_0 = 2,
	HRM_PAN_0 = 3,
	HRM_GAIN_0 = 4,
	HRM_MUTE_0 = 5,
	HRM_SOLO_0 = 6,

	HRM_DRY_PAN = 42,
	HRM_DRY_GAIN = 43,
	HRM_DRY_MUTE = 44,
	HRM_DRY_SOLO = 45,

	HRM_LATENCY = 46,
	HRM_ENABLED = 47,
	HRM_INPUT = 48,
	HRM_OUTPUT_L = 49,
	HRM_OUTPUT_R = 50
} PortIndex;


#endif // HRM_H
