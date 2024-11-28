#include "layout_model.h"

void layout_model::initialize(handleable<frame> fm)
{
	background_color = alpha_color(0, 0, 0, 0);
	fm.core->padding_left = 0.0uiabs;
	fm.core->padding_bottom = 0.0uiabs;
	fm.core->padding_right = 0.0uiabs;
	fm.core->padding_top = 0.0uiabs;
}

void layout_model::render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	rectangle viewport = fm.core->frame_viewport(),
		content_viewport = fm.core->frame_content_viewport();
	if(!fm.core->visible
		|| background_color.a == 0
		|| viewport.extent.x <= 0 || viewport.extent.y <= 0
		|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0)
		return;
	gd->br.switch_solid_color(background_color);
	gd->fill_area(viewport, bmp);
}
