#pragma once
#include "frame.h"

void hover_frame(handleable<frame> fm);

handleable<frame> hovered_frame();

void focus_frame(handleable<frame> fm);

handleable<frame> focused_frame();

void pull_frame(handleable<frame> fm);

handleable<frame> pulled_frame();

struct window
{
	frame fm;
	bitmap bmp;
	handleable<frame> layout;
	void *handler;

	window();
	~window();
	void update();
	void open();
	void close();
	void hide();
};
