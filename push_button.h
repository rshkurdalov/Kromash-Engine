#pragma once
#include "frame.h"
#include "text_field.h"

struct push_button_data
{
	void (*button_click)(void *data);
	void *data;

	void initialize(text_field_data *tf_data);
	void mouse_click(handleable<frame> fm);
	void mouse_release(handleable<frame> fm);
};

struct push_button_model
{
	bitmap inner_surface;
	bitmap border_surface;
	vector<uint32, 2> rendering_size;
	
	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, bitmap_processor *bp, bitmap *bmp);
};

struct push_button
{
	frame fm;
	text_field_data tf_data;
	push_button_data pb_data;
	push_button_model model;

	push_button();
};
