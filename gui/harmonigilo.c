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
#define DIAL_CX 40.0
#define DIAL_CY 15.0
#define ARROW_LENGTH 7.5

#define RTK_URI HRM_URI
#define RTK_GUI "ui"

typedef struct {
	LV2UI_Write_Function write;
	LV2UI_Controller controller;

	RobWidget* hbox;
	RobWidget* ctable;

	RobTkLbl* lbl_left;
	RobTkLbl* lbl_right;

	RobTkDial* pitch_L;
	RobTkDial* pitch_R;

	RobTkLbl* lbl_pitch_L;
	RobTkLbl* lbl_pitch_R;

	RobTkDial* delay_L;
	RobTkDial* delay_R;

	RobTkLbl* lbl_delay_L;
	RobTkLbl* lbl_delay_R;

	RobTkDial* panner_width;
	RobTkLbl* lbl_panner_width;

	RobTkDial* dry_wet;
	RobTkLbl* lbl_dry_wet;

	RobTkDarea* left_darea;
	RobTkDarea* right_darea;

	cairo_surface_t* bg_pitch_L;
	cairo_surface_t* bg_pitch_R;
	cairo_surface_t* bg_delay_L;
	cairo_surface_t* bg_delay_R;
	cairo_surface_t* bg_panner_width;
	cairo_surface_t* bg_dry_wet;

	PangoFontDescription* annotation_font;

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
dial_annotation_wet(RobTkDial* d, cairo_t* cr, void *data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	char buf[16];
	snprintf(buf, 16, "%3.0f %% wet", d->cur*100.0);
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
}

static void
draw_route_right(cairo_t* cr, void *handle)
{
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
}

static void
draw_route_middle(HarmonigiloUI* ui)
{
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
}

static bool cb_set_pitch_L(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->pitch_L);
	ui->write(ui->controller, HRM_PITCH_L, sizeof(float), 0, (const void*) &val);
	return true;
}

static bool cb_set_pitch_R(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->pitch_R);
	ui->write(ui->controller, HRM_PITCH_R, sizeof(float), 0, (const void*) &val);
	return true;
}

static bool cb_set_delay_L(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->delay_L);
	ui->write(ui->controller, HRM_DELAY_L, sizeof(float), 0, (const void*) &val);
	return true;
}

static bool cb_set_delay_R(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->delay_R);
	ui->write(ui->controller, HRM_DELAY_R, sizeof(float), 0, (const void*) &val);
	return true;
}

static bool cb_set_dry_wet(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->dry_wet);
	ui->write(ui->controller, HRM_DRYWET, sizeof(float), 0, (const void*) &val);
	return true;
}

static bool cb_set_panner_width(RobWidget* handle, void* data)
{
	HarmonigiloUI* ui = (HarmonigiloUI*) data;
	const float val = robtk_dial_get_value(ui->panner_width);
	ui->write(ui->controller, HRM_PANNER_WIDTH, sizeof(float), 0, (const void*) &val);

	robtk_darea_redraw(ui->left_darea);
	robtk_darea_redraw(ui->right_darea);
	draw_route_middle(ui);
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

	create_faceplate(ui);
	ui->ctable = rob_table_new(/*rows*/ 6, /*cols*/ 5, FALSE);
	ui->ctable->expose_event = box_expose_event;

	ui->lbl_left = robtk_lbl_new("Left");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_left), 1,2, 0,1, 0,20,RTK_EXPAND,RTK_SHRINK);
	ui->lbl_right = robtk_lbl_new("Right");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_right), 3,4, 0,1, 0,20,RTK_EXPAND,RTK_SHRINK);

	ui->pitch_L = make_sized_robtk_dial(-100.0, 100.0, 1.0);
	robtk_dial_set_callback(ui->pitch_L, cb_set_pitch_L, ui);
	robtk_dial_annotation_callback(ui->pitch_L, dial_annotation_pitch, ui);
	robtk_dial_set_surface(ui->pitch_L, ui->bg_pitch_L);
	ui->pitch_R = make_sized_robtk_dial(-100.0, 100.0, 1.0);
	robtk_dial_set_callback(ui->pitch_R, cb_set_pitch_R, ui);
	robtk_dial_annotation_callback(ui->pitch_R, dial_annotation_pitch, ui);
	robtk_dial_set_surface(ui->pitch_R, ui->bg_pitch_R);

	rob_table_attach(ui->ctable, robtk_dial_widget(ui->pitch_L), 1,2, 1,2, 0,0,RTK_EXPAND,RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_dial_widget(ui->pitch_R), 3,4, 1,2, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->lbl_pitch_L = robtk_lbl_new("Pitch left");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_pitch_L), 0,1, 1,2, 0,0,RTK_EXPAND,RTK_SHRINK) ;
	ui->lbl_pitch_R = robtk_lbl_new("Pitch right");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_pitch_R), 4,5, 1,2, 0,0,RTK_EXPAND,RTK_SHRINK) ;

	ui->delay_L = make_sized_robtk_dial(0.0, 50.0, 1.0);
	robtk_dial_set_callback(ui->delay_L, cb_set_delay_L, ui);
	robtk_dial_annotation_callback(ui->delay_L, dial_annotation_ms, ui);
	robtk_dial_set_surface(ui->delay_L, ui->bg_delay_L);
	ui->delay_R = make_sized_robtk_dial(0.0, 50.0, 1.0);
	robtk_dial_set_callback(ui->delay_R, cb_set_delay_R, ui);
	robtk_dial_annotation_callback(ui->delay_R, dial_annotation_ms, ui);
	robtk_dial_set_surface(ui->delay_R, ui->bg_delay_R);

	rob_table_attach(ui->ctable, robtk_dial_widget(ui->delay_L), 1,2, 2,3, 0,0,RTK_EXPAND,RTK_SHRINK);
	rob_table_attach(ui->ctable, robtk_dial_widget(ui->delay_R), 3,4, 2,3, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->lbl_delay_L = robtk_lbl_new("Delay left");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_delay_L), 0,1, 2,3, 10,0,RTK_EXPAND,RTK_SHRINK) ;
	ui->lbl_delay_R = robtk_lbl_new("Delay right");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_delay_R), 4,5, 2,3, 10,0,RTK_EXPAND,RTK_SHRINK) ;

	ui->panner_width = make_sized_robtk_dial(0.0, 1.0, 0.01);
	robtk_dial_set_callback(ui->panner_width, cb_set_panner_width, ui);

	rob_table_attach(ui->ctable, robtk_dial_widget(ui->panner_width), 2,3, 3,4, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->lbl_panner_width = robtk_lbl_new("Panner width");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_panner_width), 0,1, 3,4, 0,0,RTK_EXPAND,RTK_SHRINK) ;
	robtk_dial_set_surface(ui->panner_width, ui->bg_panner_width);

	ui->dry_wet = make_sized_robtk_dial(0.0, 1.0, 0.01);
	robtk_dial_set_callback(ui->dry_wet, cb_set_dry_wet, ui);
	robtk_dial_annotation_callback(ui->dry_wet, dial_annotation_wet, ui);
	robtk_dial_set_surface(ui->dry_wet, ui->bg_dry_wet);

	rob_table_attach(ui->ctable, robtk_dial_widget(ui->dry_wet), 2,3, 4,5, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->lbl_dry_wet = robtk_lbl_new("dry/wet");
	rob_table_attach(ui->ctable, robtk_lbl_widget(ui->lbl_dry_wet), 0,1, 4,5, 0,0,RTK_EXPAND,RTK_SHRINK) ;

	ui->left_darea = robtk_darea_new(ROUTE_WIDTH, 2.0*STEP_HEIGHT, draw_route_left, ui);
	rob_table_attach(ui->ctable, robtk_darea_widget(ui->left_darea), 1,2, 3,5, 0,0,RTK_EXPAND,RTK_SHRINK);

	ui->right_darea = robtk_darea_new(ROUTE_WIDTH, 2.0*STEP_HEIGHT, draw_route_right, ui);
	rob_table_attach(ui->ctable, robtk_darea_widget(ui->right_darea), 3,4, 3,5, 0,0,RTK_EXPAND,RTK_SHRINK);

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

	robtk_dial_destroy(ui->pitch_L);
	robtk_dial_destroy(ui->pitch_R);
	robtk_dial_destroy(ui->delay_L);
	robtk_dial_destroy(ui->delay_R);
	robtk_dial_destroy(ui->dry_wet);
	robtk_dial_destroy(ui->panner_width);

	robtk_lbl_destroy(ui->lbl_left);
	robtk_lbl_destroy(ui->lbl_right);

	robtk_lbl_destroy(ui->lbl_pitch_L);
	robtk_lbl_destroy(ui->lbl_pitch_R);
	robtk_lbl_destroy(ui->lbl_delay_L);
	robtk_lbl_destroy(ui->lbl_delay_R);
	robtk_lbl_destroy(ui->lbl_dry_wet);
	robtk_lbl_destroy(ui->lbl_panner_width);

	cairo_surface_destroy(ui->bg_pitch_L);
	cairo_surface_destroy(ui->bg_pitch_R);
	cairo_surface_destroy(ui->bg_delay_L);
	cairo_surface_destroy(ui->bg_delay_R);
	cairo_surface_destroy(ui->bg_dry_wet);
	cairo_surface_destroy(ui->bg_panner_width);

	rob_box_destroy(ui->ctable);
	rob_box_destroy(ui->hbox);

	robtk_darea_destroy(ui->left_darea);
	robtk_darea_destroy(ui->right_darea);

	pango_font_description_free(ui->annotation_font);

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

	switch ((PortIndex)port) {
	case HRM_DELAY_L:
		robtk_dial_set_value(ui->delay_L, val);
		break;
	case HRM_DELAY_R:
		robtk_dial_set_value(ui->delay_R, val);
		break;
	case HRM_PITCH_L:
		robtk_dial_set_value(ui->pitch_L, val);
		break;
	case HRM_PITCH_R:
		robtk_dial_set_value(ui->pitch_R, val);
		break;
	case HRM_DRYWET:
		robtk_dial_set_value(ui->dry_wet, val);
		break;
	case HRM_PANNER_WIDTH:
		robtk_dial_set_value(ui->panner_width, val);
		break;
	default:
		break;
	}
}
