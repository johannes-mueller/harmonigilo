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
 	HRM_DELAY_0 = 0,
	HRM_PITCH_0 = 1,
	HRM_PAN_0 = 2,
	HRM_GAIN_0 = 3,
	HRM_MUTE_0 = 4,
	HRM_SOLO_0 = 5,

	HRM_DRY_PAN = 36,
	HRM_DRY_GAIN = 37,
	HRM_DRY_MUTE = 38,
	HRM_DRY_SOLO = 39,

	HRM_LATENCY = 40,
	HRM_ENABLED = 41,
	HRM_INPUT = 42,
	HRM_OUTPUT_L = 43,
	HRM_OUTPUT_R = 44
} PortIndex;


#endif // HRM_H
