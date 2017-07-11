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
#include <string.h>

#include <stdio.h> // for debug outputs

#include <rubberband/rubberband-c.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define HRM_URI "http://johannes-mueller.org/oss/lv2/harmonigilo"

#define MAXDELAY 1000.0

typedef enum {
	HRM_DELAY_A = 0,
	HRM_PITCH_A = 1,
	HRM_INPUT = 2,
	HRM_OUTPUT = 3
} PortIndex;

typedef struct {
	const float* delayA;
	const float* pitchA;
	const float* input;
	float* output;

	float* retrieve_buffer;

	float* pitch_buffer_A;
	size_t pbufA_avail;

	double rate;

	size_t buflen;
	size_t buffer_pos;

	float* buffer1;

	RubberBandState pitcherA;
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

	float* rbuf = (float*)malloc(sizeof(float) * 8192);
	if (!rbuf) {
		return NULL;
	}

	float* pbufA = (float*)malloc(sizeof(float) * 8192);
	if (!pbufA) {
		return NULL;
	}

	Harmonigilo* hrm = (Harmonigilo*)malloc(sizeof(Harmonigilo));
	hrm->retrieve_buffer = rbuf;
	hrm->pitch_buffer_A = pbufA;
	hrm->buflen = buflen;
	hrm->buffer1 = buf;
	hrm->rate = rate;

	hrm->pitcherA = rubberband_new((unsigned int) rint(rate), 1,
				       RubberBandOptionProcessRealTime | RubberBandOptionPitchHighConsistency| RubberBandOptionPhaseIndependent | RubberBandOptionTransientsSmooth, 1.0, 1.0);
	//rubberband_set_debug_level(hrm->pitcherA, 3);
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
	case HRM_PITCH_A:
		hrm->pitchA = (const float*)data;
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
	printf("Activate called\n");
	bzero(hrm->buffer1, sizeof(float) * hrm->buflen);
	bzero(hrm->retrieve_buffer, sizeof(float) * 8192);
	bzero(hrm->pitch_buffer_A, sizeof(float) * 8192);
	hrm->pbufA_avail = 0;
	hrm->buffer_pos = 0;
}


static void
run(LV2_Handle instance, uint32_t n_samples)
{
	assert (n_samples <= 8192);
	//printf("nsamples: %d\n", n_samples);
	Harmonigilo* hrm = (Harmonigilo*)instance;

	const float* const input  = hrm->input;
	float* const output = hrm->output;
	uint32_t processed = 0;

	const float scaleA = pow(2.0, (*hrm->pitchA)/1200);

	rubberband_set_pitch_scale(hrm->pitcherA, scaleA);

	const float* proc_ptr = input;
	float* out_ptr = hrm->pitch_buffer_A;

	int cnt = 0;
	static int inst = 0;
	inst++;
	while (processed < n_samples) {
		//printf ("step: %d: instance: %d; processed: %d; ", cnt++, inst, processed);
		uint32_t in_chunk_size = rubberband_get_samples_required(hrm->pitcherA);
		uint32_t samples_left = n_samples-processed;
		//printf("required: %d; left: %d ",  in_chunk_size, samples_left);
		if (samples_left < in_chunk_size) {
			in_chunk_size = samples_left;
		}
		//printf("in_chunk_size: %d ", in_chunk_size);
		rubberband_process(hrm->pitcherA, &proc_ptr, in_chunk_size, 0);

		processed += in_chunk_size;
		proc_ptr += in_chunk_size;
		//printf("-- ");
		uint32_t avail = rubberband_available(hrm->pitcherA);
		//printf ("avail: %d; ", avail);
		if (avail+hrm->pbufA_avail > n_samples) {
			avail = n_samples - hrm->pbufA_avail;
		}
		uint32_t out_chunk_size = rubberband_retrieve(hrm->pitcherA, &(hrm->retrieve_buffer), avail);
		//printf ("out_chunk_size: %d; sum avail: %d\n", out_chunk_size, hrm->pbufA_avail+out_chunk_size);
		//memcpy (out_ptr, hrm->retrieve_buffer, out_chunk_size * sizeof(float));
		for (unsigned int i=0; i<out_chunk_size; i++) {
			out_ptr[i] = hrm->retrieve_buffer[i];
		}

		out_ptr += out_chunk_size;
		hrm->pbufA_avail += out_chunk_size;
	}

	if (!hrm->pbufA_avail) {
		return;
	}

	uint32_t actual_n_samples = hrm->pbufA_avail;
	if (n_samples < actual_n_samples) {
		actual_n_samples = n_samples;
	}

	const float delayA = *(hrm->delayA);

	size_t delay_samples = (size_t) rint(delayA*hrm->rate/1000.0);
	if (delay_samples >= hrm->buflen) {
		delay_samples = hrm->buflen - 1;
	}
	if (actual_n_samples != n_samples)
		printf("actual_samples: %d, n_samples %d\n", actual_n_samples, n_samples);
	for (uint32_t pos = 0; pos < actual_n_samples; pos++) {
		hrm->buffer1[hrm->buffer_pos] = hrm->pitch_buffer_A[pos];

		const uint32_t actual_pos = n_samples-actual_n_samples + pos;
		if (delay_samples > hrm->buffer_pos) {
			output[actual_pos] = hrm->buffer1[hrm->buflen-(delay_samples-hrm->buffer_pos)];
		} else {
			output[actual_pos] = hrm->buffer1[hrm->buffer_pos-delay_samples];
		}

		hrm->buffer_pos++;
		if (hrm->buffer_pos == hrm->buflen) {
			hrm->buffer_pos = 0;
		}
	}
	hrm->pbufA_avail -= actual_n_samples;
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;
	free(hrm->retrieve_buffer);
	free(hrm->pitch_buffer_A);
	rubberband_delete(hrm->pitcherA);
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
