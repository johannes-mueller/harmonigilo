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

#include "harmonigilo.h"

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
	char name[16];
	const float* delay;
	const float* pitch;
	float* output;

	RubberBandState pitcher;
	SampleBuffer* pitch_buffer;
	uint32_t avail;

	SampleBuffer* delay_buffer;
	uint32_t delay_samples;
	uint32_t buffer_pos;
} Channel;

typedef struct {
	const float* input;
	const float* panner_width;
	const float* dry_wet;
	float* latency;

	SampleBuffer* copied_input;
	SampleBuffer* retrieve_buffer;

	double rate;

	size_t delay_buflen;

	Channel left, right;
} Harmonigilo;


static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
	    double rate,
	    const char* bundle_path,
	    const LV2_Feature* const* features)
{
	Harmonigilo* hrm = (Harmonigilo*)malloc(sizeof(Harmonigilo));
	hrm->copied_input = new_sample_buffer(8192);
	hrm->retrieve_buffer = new_sample_buffer(8192);
	hrm->left.pitch_buffer = new_sample_buffer(8192);
	hrm->right.pitch_buffer = new_sample_buffer(8192);
	const size_t delay_buflen = (size_t) rint (rate * MAXDELAY / 1000.0);
	hrm->delay_buflen = delay_buflen;
	hrm->left.delay_buffer = new_sample_buffer(delay_buflen);
	hrm->right.delay_buffer = new_sample_buffer(delay_buflen);
	hrm->rate = rate;

	enum RubberBandOption opt =
		RubberBandOptionProcessRealTime |
		RubberBandOptionPitchHighConsistency |
		RubberBandOptionPhaseIndependent |
 		RubberBandOptionTransientsSmooth |
		RubberBandOptionWindowStandard;

	hrm->left.pitcher = rubberband_new((unsigned int) rint(rate), 1, opt, 1.0, 1.0);
	hrm->right.pitcher = rubberband_new((unsigned int) rint(rate), 1, opt, 1.0, 1.0);
	rubberband_set_debug_level(hrm->left.pitcher, 1);

	strcpy(hrm->left.name, "left");
	strcpy(hrm->right.name, "right");

	return (LV2_Handle)hrm;
}

static void
connect_port(LV2_Handle instance,
	     uint32_t   port,
	     void*      data)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;
	switch ((PortIndex)port) {
	case HRM_DELAY_L:
		hrm->left.delay = (const float*)data;
		break;
	case HRM_PITCH_L:
		hrm->left.pitch = (const float*)data;
		break;
	case HRM_DELAY_R:
		hrm->right.delay = (const float*)data;
		break;
	case HRM_PITCH_R:
		hrm->right.pitch = (const float*)data;
		break;
	case HRM_PANNER_WIDTH:
		hrm->panner_width = (const float*)data;
		break;
	case HRM_DRYWET:
		hrm->dry_wet = (const float*)data;
		break;
	case HRM_LATENCY:
		hrm->latency = (float*)data;
		break;
	case HRM_INPUT:
		hrm->input = (const float*)data;
		break;
	case HRM_OUTPUT_L:
		hrm->left.output = (float*)data;
		break;
	case HRM_OUTPUT_R:
		hrm->right.output = (float*)data;
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
	reset_sample_buffer(hrm->retrieve_buffer);
	reset_sample_buffer(hrm->left.delay_buffer);
	reset_sample_buffer(hrm->left.pitch_buffer);
	reset_sample_buffer(hrm->right.delay_buffer);
	reset_sample_buffer(hrm->right.pitch_buffer);

	hrm->left.avail = 0;
	hrm->right.avail = 0;

	hrm->left.buffer_pos = 0;
	hrm->right.buffer_pos = 0;
}

static void
pitch_shift(Harmonigilo* hrm, Channel* ch, uint32_t n_samples)
{
	uint32_t processed = 0;

	const float* proc_ptr = hrm->copied_input->data;
	float* out_ptr = ch->pitch_buffer->data;

	while (processed < n_samples) {
		uint32_t in_chunk_size = rubberband_get_samples_required(ch->pitcher);
		uint32_t samples_left = n_samples-processed;

		if (samples_left < in_chunk_size) {
			in_chunk_size = samples_left;
		}

		rubberband_process(ch->pitcher, &proc_ptr, in_chunk_size, 0);

		processed += in_chunk_size;
		proc_ptr += in_chunk_size;

		uint32_t avail = rubberband_available(ch->pitcher);

		if (avail+ch->avail > n_samples) {
			avail = n_samples - ch->avail;
		}
		uint32_t out_chunk_size = rubberband_retrieve(ch->pitcher, &(hrm->retrieve_buffer->data), avail);

		//memcpy (out_ptr, hrm->retrieve_buffer, out_chunk_size * sizeof(float));
		for (unsigned int i=0; i<out_chunk_size; i++) {
			out_ptr[i] = hrm->retrieve_buffer->data[i];
		}

		out_ptr += out_chunk_size;
		ch->avail += out_chunk_size;
	}
}

static void
delay(Harmonigilo* hrm, Channel *ch, uint32_t n_samples, uint32_t actual_n_samples)
{
	const SampleBuffer* in = ch->pitch_buffer;
	float* out = ch->output;

	if (ch->delay_samples >= hrm->delay_buflen) {
		ch->delay_samples = hrm->delay_buflen - 1;
	}

	for (uint32_t pos = 0; pos < actual_n_samples; pos++) {
		ch->delay_buffer->data[ch->buffer_pos] = in->data[pos];

		const uint32_t actual_pos = n_samples-actual_n_samples + pos;
		if (ch->delay_samples > ch->buffer_pos) {
			out[actual_pos] = ch->delay_buffer->data[hrm->delay_buflen-(ch->delay_samples-ch->buffer_pos)];
		} else {
			out[actual_pos] = ch->delay_buffer->data[ch->buffer_pos-ch->delay_samples];
		}

		ch->buffer_pos++;
		if (ch->buffer_pos == hrm->delay_buflen) {
			ch->buffer_pos = 0;
		}
	}
}

static void
process_channel(Harmonigilo* hrm, Channel* ch, uint32_t n_samples)
{

	pitch_shift(hrm, ch, n_samples);

	uint32_t actual_n_samples = ch->avail;
	if (n_samples < actual_n_samples) {
		actual_n_samples = n_samples;
	}
	if (actual_n_samples > 0) {
		delay(hrm, ch, n_samples, actual_n_samples);
		ch->avail -= actual_n_samples;
	}
}

static void prepare_channels(Harmonigilo* hrm)
{
	rubberband_set_pitch_scale(hrm->left.pitcher, pow(2.0, (*hrm->left.pitch)/1200));
	rubberband_set_pitch_scale(hrm->right.pitcher, pow(2.0, (*hrm->right.pitch)/1200));

	uint32_t lat_L = rubberband_get_latency(hrm->left.pitcher);
	uint32_t lat_R = rubberband_get_latency(hrm->right.pitcher);

	uint32_t delay_L = (int) rint(*hrm->left.delay*hrm->rate/1000.0);
	uint32_t delay_R = (int) rint(*hrm->right.delay*hrm->rate/1000.0);

	if (lat_L > lat_R) {
		lat_L -= lat_R;
		lat_R = 0;
	} else if (lat_R < lat_L) {
		lat_R -= lat_L;
		lat_L = 0;
	}

	if (lat_L > delay_L) {
		lat_L -= delay_L;
		delay_L = 0;
	} else {
		delay_L -= lat_L;
		lat_L = 0;
	}

	if (lat_R > delay_R) {
		lat_R -= delay_R;
		delay_R = 0;
	} else {
		delay_R -= lat_R;
		lat_R = 0;
	}

	hrm->left.delay_samples = delay_L;
	hrm->right.delay_samples = delay_R;

	*hrm->latency = lat_L > lat_R ? lat_L : lat_R;
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	assert (n_samples <= 8192);

	Harmonigilo* hrm = (Harmonigilo*)instance;
	memcpy (hrm->copied_input->data, hrm->input, n_samples*sizeof(float));

	prepare_channels(hrm);

	process_channel(hrm, &hrm->left, n_samples);
	process_channel(hrm, &hrm->right, n_samples);

	const float dry_wet = *hrm->dry_wet;
	const float panning = (1.0-*hrm->panner_width)/2.0;
	for (uint32_t i=0; i < n_samples; i++) {
		const float l = hrm->left.output[i];
		const float r = hrm->right.output[i];
		hrm->left.output[i] = (l*panning + r*(1.0-panning))*dry_wet + hrm->copied_input->data[i]*(1.0-dry_wet);
		hrm->right.output[i] = (r*panning + l*(1.0-panning))*dry_wet + hrm->copied_input->data[i]*(1.0-dry_wet);
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
	rubberband_delete(hrm->left.pitcher);
	rubberband_delete(hrm->right.pitcher);
	delete_sample_buffer(hrm->retrieve_buffer);
	delete_sample_buffer(hrm->left.pitch_buffer);
	delete_sample_buffer(hrm->left.delay_buffer);
	delete_sample_buffer(hrm->right.pitch_buffer);
	delete_sample_buffer(hrm->right.delay_buffer);
	free(instance);
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	HRM_URI "lv2",
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
