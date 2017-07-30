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
 	HRM_DELAY_1 = 0,
	HRM_PITCH_1 = 1,
        HRM_PAN_1 = 2,
        HRM_GAIN_1 = 3,
 	HRM_DELAY_2 = 4,
	HRM_PITCH_2 = 5,
        HRM_PAN_2 = 6,
        HRM_GAIN_2 = 7,
 	HRM_DELAY_3 = 8,
	HRM_PITCH_3 = 9,
        HRM_PAN_3 = 10,
        HRM_GAIN_3 = 11,
 	HRM_DELAY_4 = 12,
	HRM_PITCH_4 = 13,
        HRM_PAN_4 = 14,
        HRM_GAIN_4 = 15,
 	HRM_DELAY_5 = 16,
	HRM_PITCH_5 = 17,
        HRM_PAN_5 = 18,
        HRM_GAIN_5 = 19,
 	HRM_DELAY_6 = 20,
	HRM_PITCH_6 = 21,
        HRM_PAN_6 = 22,
        HRM_GAIN_6 = 23,

        HRM_DRY_PAN = 24,
	HRM_DRY_GAIN = 25,

	HRM_LATENCY = 26,
        HRM_ENABLED = 27,
	HRM_INPUT = 28,
	HRM_OUTPUT_L = 29,
	HRM_OUTPUT_R = 30
} PortIndex;


#endif // HRM_H
