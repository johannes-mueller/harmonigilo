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
	float* data;
	size_t len;
} SampleBuffer;

static SampleBuffer*
new_sample_buffer(size_t len)
{
	float* data = (float*)malloc(len*sizeof(float));
	if (!data) {
		return NULL;
	}
	SampleBuffer* sb = (SampleBuffer*)malloc(sizeof(SampleBuffer));
	if (!sb) {
		return NULL;
	}
	sb->data = data;
	sb->len = len;
	return sb;
}

static void
delete_sample_buffer(SampleBuffer* sb)
{
	free(sb->data);
	free(sb);
}

static void
reset_sample_buffer(SampleBuffer* sb)
{
	bzero(sb->data, sb->len*sizeof(float));
}

typedef struct {
	const float* delayA;
	const float* pitchA;
	const float* input;
	float* output;

	SampleBuffer* retrieve_buffer;
	SampleBuffer* pitch_buffer_A;
	uint32_t pbufA_avail;

	double rate;

	size_t delay_buflen;
	size_t buffer_pos;

	SampleBuffer* delay_buffer_A;

	RubberBandState pitcherA;
} Harmonigilo;


static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
	    double rate,
	    const char* bundle_path,
	    const LV2_Feature* const* features)
{
	Harmonigilo* hrm = (Harmonigilo*)malloc(sizeof(Harmonigilo));
	hrm->retrieve_buffer = new_sample_buffer(8192);
	hrm->pitch_buffer_A = new_sample_buffer(8192);
	hrm->delay_buflen = (size_t) rint (rate * MAXDELAY / 1000.0);
	hrm->delay_buffer_A = new_sample_buffer(hrm->delay_buflen);
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
	reset_sample_buffer(hrm->delay_buffer_A);
	reset_sample_buffer(hrm->retrieve_buffer);
	reset_sample_buffer(hrm->pitch_buffer_A);
	hrm->pbufA_avail = 0;
	hrm->buffer_pos = 0;
}

static void
pitch_shift(const float* const input, uint32_t n_samples, uint32_t* samples_available, RubberBandState pitcher, SampleBuffer* retr, SampleBuffer* out)
{
	uint32_t processed = 0;

	const float* proc_ptr = input;
	float* out_ptr = out->data;

	while (processed < n_samples) {
		uint32_t in_chunk_size = rubberband_get_samples_required(pitcher);
		uint32_t samples_left = n_samples-processed;

		if (samples_left < in_chunk_size) {
			in_chunk_size = samples_left;
		}

		rubberband_process(pitcher, &proc_ptr, in_chunk_size, 0);

		processed += in_chunk_size;
		proc_ptr += in_chunk_size;

		uint32_t avail = rubberband_available(pitcher);

		if (avail+*samples_available > n_samples) {
			avail = n_samples - *samples_available;
		}
		uint32_t out_chunk_size = rubberband_retrieve(pitcher, &(retr->data), avail);

		//memcpy (out_ptr, retr, out_chunk_size * sizeof(float));
		for (unsigned int i=0; i<out_chunk_size; i++) {
			out_ptr[i] = retr->data[i];
		}

		out_ptr += out_chunk_size;
		*samples_available += out_chunk_size;
	}
}

static uint32_t
delay(const SampleBuffer* const in, uint32_t n_samples, SampleBuffer* delay_buffer, uint32_t actual_n_samples, float delay, const Harmonigilo* hrm, float* const out)
{
	size_t delay_samples = (size_t) rint(delay*hrm->rate/1000.0);
	uint32_t buffer_pos = hrm->buffer_pos;

	if (delay_samples >= hrm->delay_buflen) {
		delay_samples = hrm->delay_buflen - 1;
	}

	for (uint32_t pos = 0; pos < actual_n_samples; pos++) {
		delay_buffer->data[buffer_pos] = in->data[pos];

		const uint32_t actual_pos = n_samples-actual_n_samples + pos;
		if (delay_samples > buffer_pos) {
			out[actual_pos] = delay_buffer->data[hrm->delay_buflen-(delay_samples-buffer_pos)];
		} else {
			out[actual_pos] = delay_buffer->data[buffer_pos-delay_samples];
		}

		buffer_pos++;
		if (buffer_pos == hrm->delay_buflen) {
			buffer_pos = 0;
		}
	}

	return buffer_pos;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	assert (n_samples <= 8192);

	Harmonigilo* hrm = (Harmonigilo*)instance;

	const float* const input  = hrm->input;
	float* const output = hrm->output;

	const float scaleA = pow(2.0, (*hrm->pitchA)/1200);

	rubberband_set_pitch_scale(hrm->pitcherA, scaleA);


	pitch_shift(input, n_samples, &hrm->pbufA_avail, hrm->pitcherA, hrm->retrieve_buffer, hrm->pitch_buffer_A);

	if (!hrm->pbufA_avail) {
		return;
	}

	uint32_t actual_n_samples = hrm->pbufA_avail;
	if (n_samples < actual_n_samples) {
		actual_n_samples = n_samples;
	}

	hrm->buffer_pos = delay(hrm->pitch_buffer_A, n_samples, hrm->delay_buffer_A, actual_n_samples, *hrm->delayA, hrm, output);

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
	rubberband_delete(hrm->pitcherA);
	delete_sample_buffer(hrm->retrieve_buffer);
	delete_sample_buffer(hrm->pitch_buffer_A);
	delete_sample_buffer(hrm->delay_buffer_A);
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
