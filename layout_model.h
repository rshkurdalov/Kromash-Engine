#pragma once
#include "frame.h"

struct layout_model
{
	alpha_color background_color;

	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp);
};
