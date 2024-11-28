#pragma once
#include "frame.h"
#include "text_field.h"
#include "flow_layout.h"
#include "function.h"

struct push_button_data
{
	function<void()> button_click;

	void initialize(text_field_data *tf_data);
	void mouse_click(handleable<frame> fm);
};

struct push_button_model
{
	bitmap inner_surface;
	bitmap border_surface;
	vector<uint32, 2> rendering_size;
	
	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp);
};

struct push_button
{
	frame fm;
	text_field_data tf_data;
	push_button_data pb_data;
	push_button_model model;

	push_button();
};

struct layout_button
{
	frame fm;
	push_button button;
	flow_layout layout;

	layout_button();
};

struct radio_button_group
{
	indefinite<frame> picked;

	radio_button_group();
};

struct radio_button_data
{
	radio_button_group *group;
};

struct radio_button_model
{
	uint32 flag_size;
	bitmap surface;
	vector<uint32, 2> rendering_size;
	bool cached_flag;

	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, radio_button_data *rb_data, graphics_displayer *gd, bitmap *bmp);
};

struct radio_button
{
	frame fm;
	text_field_data tf_data;
	radio_button_data rb_data;

	radio_button();
};
