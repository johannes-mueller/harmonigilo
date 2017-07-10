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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <strings.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define HRM_URI "http://johannes-mueller.org/oss/lv2/harmonigilo"

#define MAXDELAY 1000.0

typedef enum {
	HRM_DELAY_A = 0,
	HRM_INPUT = 1,
	HRM_OUTPUT = 2
} PortIndex;

typedef struct {
	const float* delayA;
	const float* input;
	float* output;

	double rate;
	size_t buflen;
	size_t buffer_pos;

	float* buffer1;
} Harmonigilo;


static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
	    double rate,
	    const char* bundle_path,
	    const LV2_Feature* const* features)
{
	size_t buflen = (size_t) rint (rate * MAXDELAY / 1000.0);
	float* buf = (float*)malloc(sizeof(float) * buflen);

	if (!buf) {
		return NULL;
	}

	Harmonigilo* hrm = (Harmonigilo*)malloc(sizeof(Harmonigilo));
	hrm->buflen = buflen;
	hrm->buffer1 = buf;
	hrm->rate = rate;

	return (LV2_Handle)hrm;
}

static void
connect_port(LV2_Handle instance,
	     uint32_t   port,
	     void*      data)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;

	switch ((PortIndex)port) {
	case HRM_DELAY_A:
		hrm->delayA = (const float*)data;
		break;
	case HRM_INPUT:
		hrm->input = (const float*)data;
		break;
	case HRM_OUTPUT:
		hrm->output = (float*)data;
		break;
	default:
		assert(0);
	}
}


static void
activate(LV2_Handle instance)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;

	bzero(hrm->buffer1, sizeof(float) * hrm->buflen);
	hrm->buffer_pos = 0;
}


static void
run(LV2_Handle instance, uint32_t n_samples)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;

	const float delayA = *(hrm->delayA);
	const float* const input  = hrm->input;
	float* const output = hrm->output;

	size_t delay_samples = (size_t) rint(delayA*hrm->rate/1000.0);
	if (delay_samples >= hrm->buflen) {
		delay_samples = hrm->buflen - 1;
	}

	for (uint32_t pos = 0; pos < n_samples; pos++) {
		hrm->buffer1[hrm->buffer_pos] = input[pos];

		if (delay_samples > hrm->buffer_pos) {
			output[pos] = hrm->buffer1[hrm->buflen-(delay_samples-hrm->buffer_pos)];
		} else {
			output[pos] = hrm->buffer1[hrm->buffer_pos-delay_samples];
		}

		hrm->buffer_pos++;
		if (hrm->buffer_pos == hrm->buflen) {
			hrm->buffer_pos = 0;
		}
	}
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;
	free(hrm->buffer1);
	free(instance);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	HRM_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:  return &descriptor;
	default: return NULL;
	}
}
