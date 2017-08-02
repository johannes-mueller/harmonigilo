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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
#include "src/harmonigilo.h"

#define ROUTE_WIDTH  80.0
#define STEP_HEIGHT 40.0
#define DIAL_RADIUS 10.0
#define DIAL_CX ROUTE_WIDTH/2
#define DIAL_CY 15.0
#define ARROW_LENGTH 7.5

#define RTK_URI HRM_URI
#define RTK_GUI "ui"

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller controller;

	RobWidget* hbox;
	RobWidget* ctable;

	RobTkCBtn* voice_enabled[CHAN_NUM];
	RobTkDial* pitch[CHAN_NUM];
	RobTkDial* delay[CHAN_NUM];
	RobTkDial* pan[CHAN_NUM];
	RobTkScale* gain[CHAN_NUM];
	RobWidget* sm_box[CHAN_NUM];
	RobTkCBtn* mute[CHAN_NUM];
	RobTkCBtn* solo[CHAN_NUM];

	RobTkLbl* lbl_dry;

	RobTkDial* dry_pan;
	RobTkScale* dry_gain;
	RobWidget* dry_sm_box;
	RobTkCBtn* dry_mute;
	RobTkCBtn* dry_solo;

	RobTkLbl* lbl_pitch;
	RobTkLbl* lbl_delay;
	RobTkLbl* lbl_pan;
	RobTkLbl* lbl_gain;


	RobTkDarea* left_darea;
	RobTkDarea* right_darea;

	cairo_surface_t* bg_pitch[CHAN_NUM];
	cairo_surface_t* bg_delay[CHAN_NUM];
	cairo_surface_t* bg_pan[CHAN_NUM];
	cairo_surface_t* bg_gain[CHAN_NUM];

	PangoFontDescription* annotation_font;

	bool disable_signals;

} HarmonigiloUI;


static bool box_expose_event(RobWidget *rw, cairo_t* cr, cairo_rectangle_t* ev)
{
	return rcontainer_expose_event_no_clear(rw, cr, ev);
}

/**
 * standalone robtk GUI
 */
static void ui_disable(LV2UI_Handle handle) { }
static void ui_enable(LV2UI_Handle handle) { }


static void
annotation_txt(HarmonigiloUI* ui, RobTkDial* d, cairo_t* cr, const char* txt) {
	int tw, th;
	cairo_save(cr);
	PangoLayout * pl = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(pl, ui->annotation_font);
	pango_layout_set_text(pl, txt, -1);
	pango_layout_get_pixel_size(pl, &tw, &th);
	cairo_translate (cr, d->w_cx, d->w_height);
	cairo_translate (cr, -tw/2.0 - 0.5, -th);
	cairo_set_source_rgba (cr, .0, .0, .0, .7);
	rounded_rectangle(cr, -1, -1, tw+3, th+1, 3);
	cairo_fill(cr);
	CairoSetSouerceRGBA(c_wht);
	pango_cairo_show_layout(cr, pl);
	g_object_unref(pl);
	cairo_restore(cr);
	cairo_new_path(cr);
}

static void
dial_annotation_pitch(RobTkDial* d, cairo_t* cr, void *data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	char buf[16];
	snprintf(buf, 16, "%3.1f cents", d->cur);
	annotation_txt(ui, d, cr, buf);
}

static void
dial_annotation_ms(RobTkDial* d, cairo_t* cr, void *data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	char buf[16];
	snprintf(buf, 16, "%3.1f ms", d->cur);
	annotation_txt(ui, d, cr, buf);
}

static void
dial_annotation_db(RobTkDial* d, cairo_t* cr, void *data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	char buf[16];
	snprintf(buf, 16, "%3.1f db", d->cur);
	annotation_txt(ui, d, cr, buf);
}

static void draw_arrow(cairo_t* cr)
{
	cairo_rel_line_to(cr, -ARROW_LENGTH/2.0, -ARROW_LENGTH);
	cairo_rel_line_to(cr, ARROW_LENGTH, 0.0);
	cairo_close_path(cr);
	cairo_fill(cr);
}

static void pitch_faceplate(cairo_surface_t* s)
{
	cairo_t* cr;
	cr = cairo_create(s);
	cairo_set_source_rgba(cr, .3, .4, .4, 1.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr, 0, 0, ROUTE_WIDTH, STEP_HEIGHT);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, ROUTE_WIDTH/2.0, 0);
	cairo_rel_line_to(cr, 0, STEP_HEIGHT);
	cairo_stroke(cr);

	cairo_move_to(cr, ROUTE_WIDTH/2.0, STEP_HEIGHT);
	draw_arrow(cr);

	cairo_destroy(cr);
}

static void delay_faceplate(cairo_surface_t* s)
{
	cairo_t* cr;
	cr = cairo_create(s);
	cairo_set_source_rgba(cr, .3, .3, .4, 1.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr, 0, 0, ROUTE_WIDTH, STEP_HEIGHT);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, ROUTE_WIDTH/2.0, 0);
	cairo_rel_line_to(cr, 0.0, DIAL_CY);
	cairo_stroke(cr);

	cairo_destroy(cr);
}

static void panner_width_faceplate(cairo_surface_t* s)
{
	cairo_t* cr = cairo_create(s);
	float c_bg[4];
	get_color_from_theme(1, c_bg);
	CairoSetSouerceRGBA(c_bg);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_rectangle(cr, 0, 0, ROUTE_WIDTH, STEP_HEIGHT);
	cairo_fill(cr);
	cairo_destroy(cr);
}

static void create_faceplate(HarmonigiloUI* ui)
{
	/*
	ui->bg_pitch_L = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	pitch_faceplate(ui->bg_pitch_L);

	ui->bg_pitch_R = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	pitch_faceplate(ui->bg_pitch_R);

	ui->bg_delay_L = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	delay_faceplate(ui->bg_delay_L);

	ui->bg_delay_R = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	delay_faceplate(ui->bg_delay_R);

	ui->bg_panner_width = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	panner_width_faceplate(ui->bg_panner_width);

	ui->bg_dry_wet = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ROUTE_WIDTH, STEP_HEIGHT);
	panner_width_faceplate(ui->bg_dry_wet);
	*/
	/*
	CairoSetSouerceRGBA(c_g60);
	cairo_move_to(cr, ROUTE_WIDTH, STEP_HEIGHT);
	cairo_move_to(cr, (DIAL_CX+ROUTE_WIDTH)/1.8, (DIAL_CY+STEP_HEIGHT)/1.8);
	cairo_rotate(cr, atan(STEP_HEIGHT/ROUTE_WIDTH));
	draw_arrow(cr);
	cairo_destroy(cr);
	*/

	/*
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);
	cairo_set_line_width(cr, 1.0);
	cairo_move_to(cr, DIAL_CX, 0.0);
	cairo_line_to(cr, DIAL_CX, DIAL_CY - DIAL_RADIUS);
	cairo_move_to(cr, DIAL_CX, DIAL_CY);
	cairo_line_to(cr, 0, STEP_HEIGHT);
	cairo_stroke(cr);
	CairoSetSouerceRGBA(c_g60);
	cairo_move_to(cr, ROUTE_WIDTH, STEP_HEIGHT);
	cairo_move_to(cr, (ROUTE_WIDTH-DIAL_CX)/1.8, (DIAL_CY+STEP_HEIGHT)/1.8);
	cairo_rotate(cr, atan(STEP_HEIGHT/ROUTE_WIDTH));
	draw_arrow(cr);
	cairo_destroy(cr);
	*/

}

static void
draw_route_left(cairo_t* cr, void *handle)
{
	/*
	HarmonigiloUI* ui = (HarmonigiloUI*) handle;

	const float val = robtk_dial_get_value(ui->panner_width);
	const float xroute = ROUTE_WIDTH * (0.5 + (1.0-val));

	float c_bg[4];
	get_color_from_theme(1, c_bg);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	CairoSetSouerceRGBA(c_bg);
	cairo_rectangle (cr, 0, 0, ROUTE_WIDTH, 2.0*STEP_HEIGHT);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);
	cairo_move_to(cr, ROUTE_WIDTH/2.0, 0);
	cairo_rel_line_to(cr, 0.0, DIAL_CY);

	if (xroute <= ROUTE_WIDTH + ARROW_LENGTH/2.0) {
		cairo_line_to(cr, xroute, DIAL_CY);
		cairo_line_to(cr, xroute, 2.0*STEP_HEIGHT);
		cairo_stroke(cr);

		cairo_move_to(cr, xroute, STEP_HEIGHT);
		draw_arrow(cr);
	} else {
		cairo_line_to(cr, ROUTE_WIDTH, DIAL_CY);
		cairo_stroke(cr);
	}
	*/
}

static void
draw_route_right(cairo_t* cr, void *handle)
{
	/*
	HarmonigiloUI* ui = (HarmonigiloUI*) handle;

	const float val = robtk_dial_get_value(ui->panner_width);
	const float xroute = ROUTE_WIDTH * (0.5 - (1.0-val));

	float c_bg[4];
	get_color_from_theme(1, c_bg);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	CairoSetSouerceRGBA(c_bg);
	cairo_rectangle (cr, 0, 0, ROUTE_WIDTH, 2.0*STEP_HEIGHT);
	cairo_fill(cr);

	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);
	cairo_move_to(cr, ROUTE_WIDTH/2.0, 0);
	cairo_rel_line_to(cr, 0.0, DIAL_CY);

	if (xroute >= -ARROW_LENGTH/2.) {
		cairo_line_to(cr, xroute, DIAL_CY);
		cairo_line_to(cr, xroute, 2.0*STEP_HEIGHT);
		cairo_stroke(cr);

		cairo_move_to(cr, xroute, STEP_HEIGHT);
		draw_arrow(cr);
	} else {
		cairo_line_to(cr, 0, DIAL_CY);
		cairo_stroke(cr);
	}
	*/
}

static void
draw_route_middle(HarmonigiloUI* ui)
{
	/*
	const float val = robtk_dial_get_value(ui->panner_width);

	const float xr1 = ROUTE_WIDTH/2.0 * (1.0-2.0*val);
	const float xr2 = ROUTE_WIDTH/2.0 * (1.0+2.0*val);

	cairo_t* cr;

	panner_width_faceplate(ui->bg_panner_width);
	panner_width_faceplate(ui->bg_dry_wet);

	if (xr1 < -ARROW_LENGTH/2.) {
		return;
	}

	cr = cairo_create(ui->bg_panner_width);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);

	cairo_move_to(cr, 0.0, DIAL_CY);
	cairo_line_to(cr, xr1, DIAL_CY);
	cairo_line_to(cr, xr1, STEP_HEIGHT);
	cairo_stroke(cr);

	cairo_move_to(cr, ROUTE_WIDTH, DIAL_CY);
	cairo_line_to(cr, xr2, DIAL_CY);
	cairo_line_to(cr, xr2, STEP_HEIGHT);
	cairo_stroke(cr);

	cairo_move_to(cr, xr1, STEP_HEIGHT);
	draw_arrow(cr);

	cairo_move_to(cr, xr2, STEP_HEIGHT);
	draw_arrow(cr);

	cairo_destroy(cr);


	cr = cairo_create(ui->bg_dry_wet);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	CairoSetSouerceRGBA(c_g60);

	cairo_move_to(cr, xr1, 0.0);
	cairo_line_to(cr, xr1, STEP_HEIGHT);
	cairo_stroke(cr);

	cairo_move_to(cr, xr2, 0.0);
	cairo_line_to(cr, xr2, STEP_HEIGHT);
	cairo_stroke(cr);

	cairo_destroy(cr);
	*/
}


static bool cb_set_voice_enabled(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const bool enabled = robtk_cbtn_get_active(ui->voice_enabled[i]);
		const float val = enabled ? 1.f : 0.f;
		robtk_dial_set_sensitive(ui->pitch[i], enabled);
		robtk_dial_set_sensitive(ui->delay[i], enabled);
		robtk_dial_set_sensitive(ui->pan[i], enabled);
		robtk_scale_set_sensitive(ui->gain[i], enabled);
		robtk_cbtn_set_sensitive(ui->mute[i], enabled);
		robtk_cbtn_set_sensitive(ui->solo[i], enabled);
		if (!ui->disable_signals) {
			ui->write(ui->controller, HRM_ENABLED_0+(7*i), sizeof(float), 0, (const void*) &val);
		}
	}
	return true;
}


static bool cb_set_pitch(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_dial_get_value(ui->pitch[i]);
		ui->write(ui->controller, HRM_PITCH_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_delay(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_dial_get_value(ui->delay[i]);
		ui->write(ui->controller, HRM_DELAY_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_pan(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_dial_get_value(ui->pan[i]);
		ui->write(ui->controller, HRM_PAN_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_gain(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_scale_get_value(ui->gain[i]);
		ui->write(ui->controller, HRM_GAIN_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_mute(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_cbtn_get_active(ui->mute[i]) ? 1.f : 0.f;
		ui->write(ui->controller, HRM_MUTE_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_solo(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		const float val = robtk_cbtn_get_active(ui->solo[i]) ? 1.f : 0.f;
		ui->write(ui->controller, HRM_SOLO_0+(7*i), sizeof(float), 0, (const void*) &val);
	}
	return true;
}

static bool cb_set_dry_pan(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	const float val = robtk_dial_get_value(ui->dry_pan);
	ui->write(ui->controller, HRM_DRY_PAN, sizeof(float), 0, (const void*) &val);

	return true;
}

static bool cb_set_dry_gain(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	const float val = robtk_scale_get_value(ui->dry_gain);
	ui->write(ui->controller, HRM_DRY_GAIN, sizeof(float), 0, (const void*) &val);

	return true;
}


static bool cb_set_dry_mute(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	const float val = robtk_cbtn_get_active(ui->dry_mute) ? 1.f : 0.f;
	ui->write(ui->controller, HRM_DRY_MUTE, sizeof(float), 0, (const void*) &val);

	return true;
}


static bool cb_set_dry_solo(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	if (ui->disable_signals) {
		return true;
	}
	const float val = robtk_cbtn_get_active(ui->dry_solo) ? 1.f : 0.f;
	ui->write(ui->controller, HRM_DRY_SOLO, sizeof(float), 0, (const void*) &val);

	return true;
}

static RobTkDial* make_sized_robtk_dial(float min, float max, float step)
{
	return robtk_dial_new_with_size(min, max, step, ROUTE_WIDTH, STEP_HEIGHT, DIAL_CX, DIAL_CY, DIAL_RADIUS);
}

static RobWidget* setup_toplevel(HarmonigiloUI* ui)
{
	ui->hbox = rob_hbox_new(FALSE, 2);

	ui->annotation_font = pango_font_description_from_string("Mono 10px");

	//create_faceplate(ui);
	ui->ctable = rob_table_new(/*rows*/ 8, /*cols*/ CHAN_NUM+2, FALSE);
	ui->ctable->expose_event = box_expose_event;

	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		char txt[16];
		sprintf(txt, "Voice %d", i+1);
		ui->voice_enabled[i] = robtk_cbtn_new(txt, GBT_LED_LEFT, false);
		robtk_cbtn_set_callback(ui->voice_enabled[i], cb_set_voice_enabled, ui);
		rob_table_attach(ui->ctable, robtk_cbtn_widget(ui->voice_enabled[i]), i+1, i+2, 0, 1, 0,0,RTK_EXPAND,RTK_SHRINK);

		ui->pitch[i] = make_sized_robtk_dial(-100.0, 100.0, 1.0);
		robtk_dial_set_default(ui->pitch[i], 0.0);
		robtk_dial_set_callback(ui->pitch[i], cb_set_pitch, ui);
		robtk_dial_annotation_callback(ui->pitch[i], dial_annotation_pitch, ui);
		//robtk_dial_set_surface(ui->pitch[i], ui->bg_pitch[i]);
		rob_table_attach(ui->ctable, robtk_dial_widget(ui->pitch[i]), i+1,i+2, 1, 2, 0,0,RTK_EXPAND,RTK_SHRINK);

		ui->delay[i] = make_sized_robtk_dial(0.0, 50.0, 1.0);
		robtk_dial_set_default(ui->delay[i], 0.0);
		robtk_dial_set_callback(ui->delay[i], cb_set_delay, ui);
		robtk_dial_annotation_callback(ui->delay[i], dial_annotation_ms, ui);
		//robtk_dial_set_surface(ui->delay[i], ui->bg_delay[i]);
		rob_table_attach(ui->ctable, robtk_dial_widget(ui->delay[i]), i+1,i+2, 2,3, 0,0,RTK_EXPAND,RTK_SHRINK);

		ui->pan[i] = make_sized_robtk_dial(0.0, 1.0, 0.05);
		robtk_dial_set_default(ui->pan[i], 0.5);
		robtk_dial_set_callback(ui->pan[i], cb_set_pan, ui);
		//robtk_dial_set_surface(ui->pan[i], ui->bg_pan[i]);
		rob_table_attach(ui->ctable, robtk_dial_widget(ui->pan[i]), i+1,i+2, 3,4, 0,0,RTK_EXPAND,RTK_SHRINK);

		ui->sm_box[i] = rob_vbox_new(FALSE, 2);

		ui->mute[i] = robtk_cbtn_new("Mute", GBT_LED_LEFT, false);
		robtk_cbtn_set_color_on(ui->mute[i], 1.f, 1.f, 0.f);
		robtk_cbtn_set_color_off(ui->mute[i], 0.2f, 0.2f, 0.f);
		robtk_cbtn_set_callback(ui->mute[i], cb_set_mute, ui);
		rob_vbox_child_pack(ui->sm_box[i], robtk_cbtn_widget(ui->mute[i]), false, false);

		ui->solo[i] = robtk_cbtn_new("Solo", GBT_LED_LEFT, false);
		robtk_cbtn_set_color_on(ui->solo[i], 0.f, 1.f, 0.f);
		robtk_cbtn_set_color_off(ui->solo[i], 0.0f, 0.2f, 0.f);
		robtk_cbtn_set_callback(ui->solo[i], cb_set_solo, ui);
		rob_vbox_child_pack(ui->sm_box[i], robtk_cbtn_widget(ui->solo[i]), false, false);

		rob_table_attach(ui->ctable, ui->sm_box[i], i+1, i+2, 4,5, 0,0,RTK_EXPAND,RTK_SHRINK);

		ui->gain[i] = robtk_scale_new(-60.f, +6.f, 1.0, false);
		robtk_scale_set_default(ui->gain[i], 0.0);
		robtk_scale_set_callback(ui->gain[i], cb_set_gain, ui);
		//robtk_scale_set_surface(ui->gain[i], ui->bg_gain[i]);
		rob_table_attach(ui->ctable, robtk_scale_widget(ui->gain[i]), i+1,i+2, 5,6, 0,0,RTK_EXPAND,RTK_SHRINK);
	}

	ui->dry_pan = make_sized_robtk_dial(0.0, 1.0, 0.05);
	robtk_dial_set_default(ui->dry_pan, 0.5);
	robtk_dial_set_callback(ui->dry_pan, cb_set_dry_pan, ui);
	//robtk_dial_set_surface(ui->dry_pan, ui->bg_pan);
	rob_table_attach(ui->ctable, robtk_dial_widget(ui->dry_pan), 7,8, 3,4, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->dry_sm_box = rob_vbox_new(FALSE, 2);

	ui->dry_mute = robtk_cbtn_new("Mute", GBT_LED_LEFT, false);
	robtk_cbtn_set_color_on(ui->dry_mute, 1.f, 1.f, 0.f);
	robtk_cbtn_set_color_off(ui->dry_mute, 0.2f, 0.2f, 0.f);
	robtk_cbtn_set_callback(ui->dry_mute, cb_set_dry_mute, ui);
	rob_vbox_child_pack(ui->dry_sm_box, robtk_cbtn_widget(ui->dry_mute), false, false);

	ui->dry_solo = robtk_cbtn_new("Solo", GBT_LED_LEFT, false);
	robtk_cbtn_set_color_on(ui->dry_solo, 0.f, 1.f, 0.f);
	robtk_cbtn_set_color_off(ui->dry_solo, 0.0f, 0.2f, 0.f);
	robtk_cbtn_set_callback(ui->dry_solo, cb_set_dry_solo, ui);
	rob_vbox_child_pack(ui->dry_sm_box, robtk_cbtn_widget(ui->dry_solo), false, false);

	rob_table_attach(ui->ctable, ui->dry_sm_box, 7,8, 4,5, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->dry_gain = robtk_scale_new(-60.f, +6.f, 1.0, false);
	robtk_scale_set_default(ui->dry_gain, 0.0);
	robtk_scale_set_callback(ui->dry_gain, cb_set_dry_gain, ui);
	//robtk_scale_set_surface(ui->dry_gain, ui->bg_dry_gain);
	rob_table_attach(ui->ctable, robtk_scale_widget(ui->dry_gain), 7,8, 5,6, 0,0,RTK_EXPAND,RTK_SHRINK);


	ui->lbl_dry = robtk_lbl_new("Dry");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_dry), 7,8, 0,1, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->lbl_pitch = robtk_lbl_new("Pitch");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_pitch), 0,1, 1,2, 0,0,RTK_EXPAND,RTK_SHRINK);
	ui->lbl_delay = robtk_lbl_new("Delay");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_delay), 0,1, 2,3, 0,0,RTK_EXPAND,RTK_SHRINK);
	ui->lbl_pan = robtk_lbl_new("Pan");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_pan), 0,1, 3,4, 0,0,RTK_EXPAND,RTK_SHRINK);
	ui->lbl_gain = robtk_lbl_new("Gain");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_gain), 0,1, 5,6, 0,0,RTK_EXPAND,RTK_SHRINK);

	/*
	printf("dry %x\n", robtk_lbl_widget(ui->lbl_dry));
	printf("pitch %x\n", robtk_lbl_widget(ui->lbl_pitch));
	printf("pitch df %x\n", ui->lbl_pitch->sf_txt);
	printf("delay %x\n", robtk_lbl_widget(ui->lbl_delay));
	printf("pan %x\n", robtk_lbl_widget(ui->lbl_pan));
	printf("gain %x\n", robtk_lbl_widget(ui->lbl_gain));
	*/
	rob_hbox_child_pack(ui->hbox, ui->ctable, FALSE, FALSE);

	return ui->hbox;
}

static LV2UI_Handle
instantiate(void* const ui_toplevel,
	    const LV2UI_Descriptor* descriptor,
	    const char* plugin_uri,
	    const char* bundle_path,
	    LV2UI_Write_Function write_,
	    LV2UI_Controller controller_,
	    RobWidget** widget,
	    const LV2_Feature* const* features)
{
	HarmonigiloUI* ui = (HarmonigiloUI*)calloc(1, sizeof(HarmonigiloUI));

	if (!ui) {
		fprintf(stderr, "HarmonigiloUI: out of memory\n");
		return NULL;
	}

	if (!ui_toplevel) {
		fprintf(stderr, "No toplevel widget\n");
		return NULL;
	}

	ui->write = write_;
	ui->controller = controller_;

	*widget = setup_toplevel(ui);
	robwidget_make_toplevel(ui->hbox, ui_toplevel);

	ui->disable_signals = true;

	return ui;
}




static enum LVGLResize
plugin_scale_mode(LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup(LV2UI_Handle handle)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) handle;

	for (uint32_t i=0; i<CHAN_NUM; ++i) {
		robtk_cbtn_destroy(ui->voice_enabled[i]);
		robtk_dial_destroy(ui->pitch[i]);
		robtk_dial_destroy(ui->delay[i]);
		robtk_dial_destroy(ui->pan[i]);
		robtk_scale_destroy(ui->gain[i]);
		robtk_cbtn_destroy(ui->mute[i]);
		robtk_cbtn_destroy(ui->solo[i]);
		rob_box_destroy(ui->sm_box[i]);
	}

	robtk_dial_destroy(ui->dry_pan);
	robtk_scale_destroy(ui->dry_gain);
	robtk_cbtn_destroy(ui->dry_mute);
	robtk_cbtn_destroy(ui->dry_solo);

	rob_box_destroy(ui->dry_sm_box);

	robtk_lbl_destroy(ui->lbl_dry);

	robtk_lbl_destroy(ui->lbl_pitch);
	robtk_lbl_destroy(ui->lbl_delay);
	robtk_lbl_destroy(ui->lbl_pan);
	robtk_lbl_destroy(ui->lbl_gain);

	/*
	cairo_surface_destroy(ui->bg_pitch_L);
	cairo_surface_destroy(ui->bg_pitch_R);
	cairo_surface_destroy(ui->bg_delay_L);
	cairo_surface_destroy(ui->bg_delay_R);
	cairo_surface_destroy(ui->bg_dry_wet);
	cairo_surface_destroy(ui->bg_panner_width);

	robtk_darea_destroy(ui->left_darea);
	robtk_darea_destroy(ui->right_darea);

	pango_font_description_free(ui->annotation_font);
	*/

	rob_box_destroy(ui->ctable);
	rob_box_destroy(ui->hbox);

	free(ui);
	printf("Cleaned up\n");
}

static const void*
extension_data(const char* uri)
{
	return NULL;
}

static void
port_event(LV2UI_Handle handle,
	   uint32_t     port,
	   uint32_t     buffer_size,
	   uint32_t     format,
	   const void*  buffer)
{
	if (format != 0) {
		return;
	}

	HarmonigiloUI* ui = (HarmonigiloUI*) handle;
	const float val = *(const float*)buffer;

	ui->disable_signals = true;

	const uint32_t num = port/7;
	if (num < CHAN_NUM) {
		printf("Port: %d %d %d %f\n", port, port/4, port % 4, val);
		switch ((PortIndex) (port % 7)) {
		case HRM_ENABLED_0:
			robtk_cbtn_set_active(ui->voice_enabled[num], val>0.5);
			break;
		case HRM_DELAY_0:
			robtk_dial_set_value(ui->delay[num], val);
			break;
		case HRM_PITCH_0:
			robtk_dial_set_value(ui->pitch[num], val);
			break;
		case HRM_PAN_0:
			robtk_dial_set_value(ui->pan[num], val);
			break;
		case HRM_GAIN_0:
			robtk_scale_set_value(ui->gain[num], val);
			break;
		case HRM_MUTE_0:
			robtk_cbtn_set_active(ui->mute[num], val>0.5);
			break;
		case HRM_SOLO_0:
			robtk_cbtn_set_active(ui->solo[num], val>0.5);
			break;
		default:
			break;
		}
	} else {
 		switch ((PortIndex) (port)) {
		case HRM_DRY_PAN:
			robtk_dial_set_value(ui->dry_pan, val);
			break;
		case HRM_DRY_GAIN:
			robtk_scale_set_value(ui->dry_gain, val);
			break;
		default:
			break;
		}
	}

	ui->disable_signals = false;
}
