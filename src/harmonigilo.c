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
#include <stdbool.h>
#include <strings.h>
#include <string.h>

#include <stdio.h> // for debug outputs

#include <rubberband/rubberband-c.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#include "harmonigilo.h"

#define BUFLEN 8192

#ifndef MIN
#define MIN(A,B) ( (A) < (B) ? (A) : (B) )
#endif
#ifndef MAX
#define MAX(A,B) ( (A) > (B) ? (A) : (B) )
#endif

static inline float
from_dB(float gdb) {
	return (exp(gdb/20.f*log(10.f)));
}

typedef struct {
	float* data;
	size_t len;
	size_t write_pos, read_pos;
} SampleBuffer;

static SampleBuffer*
new_sample_buffer(size_t len)
{
	float* data = (float*)calloc(len, sizeof(float));
	if (!data) {
		return NULL;
	}
	SampleBuffer* sb = (SampleBuffer*)malloc(sizeof(SampleBuffer));
	if (!sb) {
		return NULL;
	}
	sb->data = data;
	sb->len = len;
	sb->write_pos = 0;
	sb->read_pos = 0;
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
	sb->read_pos = 0;
	sb->write_pos = 0;
}

static void
put_to_sample_buffer(SampleBuffer* sb, const float* data, size_t len)
{
	assert (len <= sb->len);

	const size_t c = sb->len - sb->write_pos;
	if (c >= len) {
		memcpy (sb->data + sb->write_pos, data, len*sizeof(float));
		sb->write_pos += len;
		if (sb->write_pos == sb->len) {
			sb->write_pos = 0;
		}
	} else {
		memcpy (sb->data + sb->write_pos, data, c*sizeof(float));
		memcpy (sb->data, data+c, (len-c)*sizeof(float));
		sb->write_pos = len-c;
	}
}

static uint32_t
calc_sample_buffer_pos(const SampleBuffer* sb, int rel_pos)
{
	int pos = sb->read_pos + rel_pos;
	if (pos < 0) {
		return sb->len+pos;
	}
	if (pos >= (int)sb->len) {
		return pos - sb->len;
	}

	return pos;
}

static void
sample_buffer_advance_read_pos(SampleBuffer* sb, size_t inc)
{
	sb->read_pos += inc;
	if (sb->read_pos >= sb->len) {
		sb->read_pos -= sb->len;
	}
}

static void
get_from_sample_buffer(SampleBuffer* sb, int rel_pos, float* dst, size_t len)
{
	if (sb->write_pos == sb->read_pos) {
		memset(dst, 0, len*sizeof(float));
		return;
	}
	uint32_t pos = calc_sample_buffer_pos(sb, rel_pos);
	if (pos < sb->write_pos && pos > sb->write_pos-len) {
		const uint32_t d = len-(sb->write_pos-pos);
		memset(dst, 0, d*sizeof(float));
		memcpy(dst+d, sb->data+pos, (len-d)*sizeof(float));
		sample_buffer_advance_read_pos(sb, len-d);
		return;
	}
	if (pos+len > sb->len) {
		const size_t c = sb->len-pos;
		memcpy(dst, sb->data+pos, c*sizeof(float));
		memcpy(dst+c, sb->data, (len-c)*sizeof(float));
		sb->read_pos = (len-c-rel_pos);
	} else {
		memcpy(dst, sb->data+pos, len*sizeof(float));
		sample_buffer_advance_read_pos(sb, len);
	}
}

static float
get_sample_from_sample_buffer(SampleBuffer* sb, int rel_pos)
{
	uint32_t pos = calc_sample_buffer_pos(sb, rel_pos);
	assert(pos>=0);
	assert(pos<sb->len);
	sample_buffer_advance_read_pos(sb, 1);
	return sb->data[pos];
}



typedef struct {
	const float* enabled;
	const float* delay;
	const float* pitch;
	const float* pan;
	const float* gain;
	const float* mute;
	const float* solo;

	float* delay_buffer;

	RubberBandState pitcher;
	SampleBuffer* pitch_buffer;

	uint32_t delay_samples;

	uint32_t latency;
} Channel;

typedef struct {
	const float* input;
	float* output_L;
	float* output_R;

	const float* dry_pan;
	const float* dry_gain;
	const float* dry_mute;
	const float* dry_solo;

	float* latency;

	const float* enabled;

	float* copied_input;
	float* retrieve_buffer;

	SampleBuffer* latency_buffer;

	double rate;

	Channel channel[CHAN_NUM];
} Harmonigilo;


static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
	    double rate,
	    const char* bundle_path,
	    const LV2_Feature* const* features)
{
	Harmonigilo* hrm = (Harmonigilo*)malloc(sizeof(Harmonigilo));
	hrm->copied_input = (float*)malloc(BUFLEN*sizeof(float));
	hrm->retrieve_buffer = (float*)malloc(BUFLEN*sizeof(float));
	const size_t delay_buflen = (size_t) rint (rate * MAXDELAY / 1000.0);

	enum RubberBandOption pitch_opt =
		RubberBandOptionProcessRealTime |
		RubberBandOptionPitchHighConsistency |
		RubberBandOptionPhaseIndependent |
 		RubberBandOptionTransientsSmooth |
		RubberBandOptionWindowStandard;

	uint32_t rate_i = (uint32_t) rint(rate);
	for (Channel* ch = hrm->channel; ch < hrm->channel+CHAN_NUM; ++ch) {
		ch->pitch_buffer = new_sample_buffer(delay_buflen);
		ch->pitcher = rubberband_new(rate_i, 1, pitch_opt, 1.0, 1.0);
		ch->delay_buffer = (float*)malloc(BUFLEN*sizeof(float));
	}
	hrm->latency_buffer = new_sample_buffer(BUFLEN);
	hrm->rate = rate;

	return (LV2_Handle)hrm;
}

static void
connect_port(LV2_Handle instance,
	     uint32_t   port,
	     void*      data)
{
	Harmonigilo* hrm = (Harmonigilo*)instance;

	if (port < CHAN_NUM*7) {
		Channel* ch = &hrm->channel[port/7];
		switch (port % 7) {
		case HRM_ENABLED_0:
			ch->enabled = (const float*)data;
			break;
		case HRM_DELAY_0:
			ch->delay = (const float*)data;
			break;
		case HRM_PITCH_0:
			ch->pitch = (const float*)data;
			break;
		case HRM_PAN_0:
			ch->pan = (const float*)data;
			break;
		case HRM_GAIN_0:
			ch->gain = (const float*)data;
			break;
		case HRM_MUTE_0:
			ch->mute = (const float*)data;
			break;
		case HRM_SOLO_0:
			ch->solo = (const float*)data;
			break;
		default:
			break;
		}
		return;
	}

	switch ((PortIndex)port) {
	case HRM_DRY_PAN:
		hrm->dry_pan = (const float*)data;
		break;
	case HRM_DRY_GAIN:
		hrm->dry_gain = (const float*)data;
		break;
	case HRM_DRY_MUTE:
		hrm->dry_mute = (const float*)data;
		break;
	case HRM_DRY_SOLO:
		hrm->dry_solo = (const float*)data;
		break;
	case HRM_LATENCY:
		hrm->latency = (float*)data;
		break;
	case HRM_ENABLED:
		hrm->enabled = (float*)data;
		break;
	case HRM_INPUT:
		hrm->input = (const float*)data;
		break;
	case HRM_OUTPUT_L:
		hrm->output_L = (float*)data;
		break;
	case HRM_OUTPUT_R:
		hrm->output_R = (float*)data;
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
	bzero(hrm->copied_input, BUFLEN*sizeof(float));
	bzero(hrm->retrieve_buffer, BUFLEN*sizeof(float));
	for (Channel* ch = hrm->channel; ch < hrm->channel+CHAN_NUM; ++ch) {
		reset_sample_buffer(ch->pitch_buffer);
	}
	reset_sample_buffer(hrm->latency_buffer);
}

static void
pitch_shift(Harmonigilo* hrm, Channel* ch, uint32_t n_samples)
{
	uint32_t processed = 0;

	const float* proc_ptr = hrm->copied_input;

	while (processed < n_samples) {
		uint32_t in_chunk_size = rubberband_get_samples_required(ch->pitcher);
		uint32_t samples_left = n_samples-processed;

		if (samples_left < in_chunk_size) {
			in_chunk_size = samples_left;
		}

		rubberband_process(ch->pitcher, &proc_ptr, in_chunk_size, 0);

		processed += in_chunk_size;
		proc_ptr += in_chunk_size;

		const uint32_t avail = rubberband_available(ch->pitcher);
		const uint32_t out_chunk_size = rubberband_retrieve(ch->pitcher, &(hrm->retrieve_buffer), avail);
		put_to_sample_buffer(ch->pitch_buffer, hrm->retrieve_buffer, out_chunk_size);
	}
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
	assert (n_samples <= 8192);

	Harmonigilo* hrm = (Harmonigilo*)instance;

	if (*hrm->enabled <= 0) {
		float in = 0.f;
		for (uint32_t i=0; i<n_samples; ++i) {
			in = hrm->input[i] * 0.86070797642505780723; // -3db exp(-3.f/20.f*log(10.f))
			hrm->output_L[i] = in;
			hrm->output_R[i] = in;
		}
		return;
	}

	memcpy (hrm->copied_input, hrm->input, n_samples*sizeof(float));
	put_to_sample_buffer(hrm->latency_buffer, hrm->copied_input, n_samples);

	uint32_t min_delay = MAXDELAY*hrm->rate/1000.0;
	uint32_t max_latency = 0;

	bool solo = false;
	if (*hrm->dry_solo > 0.5) {
		solo = true;
	}

	for (Channel* ch = hrm->channel; ch < hrm->channel+CHAN_NUM; ++ch) {
		if (*ch->enabled < 0.5) {
			continue;
		}
		rubberband_set_pitch_scale(ch->pitcher, pow(2.0, (*ch->pitch)/1200));

		ch->latency = 2*rubberband_get_latency(ch->pitcher);
		ch->delay_samples = (uint32_t) rint((*ch->delay)*hrm->rate/1000.0);

		if (ch->delay_samples < min_delay) {
			min_delay = ch->delay_samples;
		}

		if (ch->latency > max_latency) {
			max_latency = ch-> latency;
		}

		if (*ch->solo > 0.5) {
			solo = true;
		}
	}

	uint32_t latency_correction;
	if (min_delay >= max_latency) {
		latency_correction = min_delay - max_latency;
		*hrm->latency = 0;
	} else {
		latency_correction = min_delay;
		*hrm->latency = max_latency - min_delay;
	}

	for (Channel* ch = hrm->channel; ch < hrm->channel+CHAN_NUM; ++ch) {
		if (*ch->enabled < 0.5) {
			continue;
		}
		ch->delay_samples -= latency_correction;

		pitch_shift(hrm, ch, n_samples);
//		printf("Delay: %f, %d\n", *ch->delay, ch->delay_samples);
		get_from_sample_buffer(ch->pitch_buffer, -(ch->delay_samples), ch->delay_buffer, n_samples);
	}

	float dry_gain = from_dB(*hrm->dry_gain);
	if ((*hrm->dry_mute>0.5) || (solo && (*hrm->dry_solo<=0.5))) {
			dry_gain = 0.f;
	}
	const float dry_pan = *hrm->dry_pan;
	for (uint32_t i=0; i<n_samples; ++i) {
		const float in = dry_gain*get_sample_from_sample_buffer(hrm->latency_buffer, -(*hrm->latency));
		hrm->output_L[i] = in*(1.f-dry_pan);
		hrm->output_R[i] = in*dry_pan;
	}


	for (Channel* ch = hrm->channel; ch < hrm->channel+CHAN_NUM; ++ch) {
		if (*ch->enabled < 0.5) {
			continue;
		}
		const float pan = *ch->pan;
		float gain = from_dB(*ch->gain);
		if ((*ch->mute>0.5) || (solo && (*ch->solo<=0.5))) {
			gain = 0.f;
		}
		for (uint32_t i=0; i<n_samples; ++i) {
			const float p = gain*ch->delay_buffer[i];
			hrm->output_L[i] += p*(1.f-pan);
			hrm->output_R[i] += p*pan;
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
	for (int i=0; i<CHAN_NUM; ++i) {
		rubberband_delete(hrm->channel[i].pitcher);
		delete_sample_buffer(hrm->channel[i].pitch_buffer);
		free (hrm->channel[i].delay_buffer);
	}
	free (hrm->copied_input);
	free (hrm->retrieve_buffer);
	delete_sample_buffer(hrm->latency_buffer);
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
