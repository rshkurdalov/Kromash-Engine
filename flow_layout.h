#pragma once
#include "frame.h"
#include "scroll_bar.h"
#include "layout_model.h"

struct flow_layout_frame
{
	handleable<frame> fm;
	horizontal_align halign;
	vertical_align valign;
	bool line_break;

	flow_layout_frame() {}

	flow_layout_frame(
		handleable<frame> fm,
		horizontal_align halign = horizontal_align::left,
		vertical_align valign = vertical_align::top,
		bool line_break = false)
		: fm(fm),
		halign(halign),
		valign(valign),
		line_break(line_break) {}
};

struct flow_layout_data
{
	array<flow_layout_frame> frames;
	flow_axis direction;
	flow_line_offset offset;
	bool multiline;
	scroll_bar xscroll;
	scroll_bar yscroll;

	void initialize(handleable<frame> fm);
	void subframes(handleable<frame> fm, array<handleable<frame>> *frames_addr);
	vector<uint32, 2> content_size(handleable<frame> fm, uint32 viewport_width, uint32 viewport_height);
	void update_layout(handleable<frame> fm);
	void render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp);
	void mouse_wheel_rotate(handleable<frame> fm);
};

struct flow_layout
{
	frame fm;
	flow_layout_data data;
	layout_model model;

	flow_layout();
};
