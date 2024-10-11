#pragma once
#include "frame.h"

struct scroll_bar_data
{
	bool vertical;
	uint32 content_size;
	uint32 viewport_size;
	uint32 viewport_offset;

	void initialize(handleable<frame> fm);
	void shift(uint32 value, bool forward);
	rectangle<int32> slider_rectangle(handleable<frame> fm);
	void mouse_click(handleable<frame> fm);
};

struct scroll_bar_model
{
	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, scroll_bar_data *data, bitmap_processor *bp, bitmap *bmp);
};

struct scroll_bar
{
	frame fm;
	scroll_bar_data data;
	scroll_bar_model model;

	scroll_bar();
};
