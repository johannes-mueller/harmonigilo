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

#define MAXDELAY 1000.0

typedef enum {
 	HRM_DELAY_L = 0,
	HRM_PITCH_L = 1,
	HRM_DELAY_R = 2,
	HRM_PITCH_R = 3,
	HRM_PANNER_WIDTH = 4,
	HRM_DRYWET = 5,
	HRM_LATENCY = 6,
        HRM_ENABLED = 7,
	HRM_INPUT = 8,
	HRM_OUTPUT_L = 9,
	HRM_OUTPUT_R = 10
} PortIndex;


#endif // HRM_H
