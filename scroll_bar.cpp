#include "scroll_bar.h"

void scroll_bar_data::initialize(handleable<frame> fm)
{
	vertical = true;
	content_size = 0;
	viewport_size = 0;
	viewport_offset = 0;
}

void scroll_bar_data::shift(uint32 value, bool forward)
{
	if(forward)
	{
		if(viewport_offset < value)
			viewport_offset = 0;
		else viewport_offset -= value;
	}
	else viewport_offset += value;
}

rectangle<int32> scroll_bar_data::slider_rectangle(handleable<frame> fm)
{
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	int32 slider_size = (vertical ? content_viewport.extent.y : content_viewport.extent.x)
		* viewport_size / content_size;
	slider_size = min(max(slider_size, 6), (vertical ? content_viewport.extent.y : content_viewport.extent.x));
	int32 slider_offset = ((vertical ? content_viewport.extent.y : content_viewport.extent.x) - slider_size)
		* viewport_offset / (content_size - viewport_size);
	rectangle<int32> slider_rect;
	if(vertical)
	{
		slider_rect.position = vector<int32, 2>(
			content_viewport.position.x,
			content_viewport.position.y + content_viewport.extent.y - slider_offset - slider_size);
		slider_rect.extent = vector<int32, 2>(content_viewport.extent.x, slider_size);
	}
	else
	{
		slider_rect.position = vector<int32, 2>(
			content_viewport.position.x + slider_offset,
			content_viewport.position.y);
		slider_rect.extent = vector<int32, 2>(slider_size, content_viewport.extent.y);
	}
	return slider_rect;
}

bool slider_rect_moving_active = false;
scroll_bar_data *moving_slider;

void slider_rect_mouse_move()
{
	if(moving_slider->vertical)
	{
		if(mouse()->position.y < mouse()->prev_position.y)
			moving_slider->shift(uint32(mouse()->prev_position.y - mouse()->position.y)
				* moving_slider->content_size / moving_slider->viewport_size,
				false);
		else moving_slider->shift(uint32(mouse()->position.y - mouse()->prev_position.y)
				* moving_slider->content_size / moving_slider->viewport_size,
				true);
	}
	else
	{
		if(mouse()->position.x < mouse()->prev_position.x)
			moving_slider->shift(uint32(mouse()->prev_position.x - mouse()->position.x)
				* moving_slider->content_size / moving_slider->viewport_size,
				false);
		else moving_slider->shift(uint32(mouse()->position.x - mouse()->prev_position.x)
				* moving_slider->content_size / moving_slider->viewport_size,
				true);
	}
}

void slider_rect_mouse_release()
{
	slider_rect_moving_active = false;
	mouse()->mouse_move.callbacks.remove_element(slider_rect_mouse_move);
	mouse()->mouse_release.callbacks.remove_element(slider_rect_mouse_release);
}

void scroll_bar_data::mouse_click(handleable<frame> fm)
{
	rectangle<int32> slider_rect = slider_rectangle(fm);
	if(slider_rect.hit_test(mouse()->position) && !slider_rect_moving_active)
	{
		slider_rect_moving_active = true;
		moving_slider = this;
		mouse()->mouse_move.callbacks.push(slider_rect_mouse_move);
		mouse()->mouse_release.callbacks.push(slider_rect_mouse_release);
	}
}

void scroll_bar_model::initialize(handleable<frame> fm)
{
	fm.core->width_desc = 12.0uiabs;
	fm.core->height_desc = 12.0uiabs;
	fm.core->margin_left = 0.0uiabs;
	fm.core->margin_bottom = 0.0uiabs;
	fm.core->margin_right = 0.0uiabs;
	fm.core->margin_top = 0.0uiabs;
	fm.core->padding_left = 2.0uiabs;
	fm.core->padding_bottom = 2.0uiabs;
	fm.core->padding_right = 2.0uiabs;
	fm.core->padding_top = 2.0uiabs;
	fm.core->return_mouse_wheel_rotate = true;
}

void scroll_bar_model::render(handleable<frame> fm, scroll_bar_data *data, graphics_displayer *gd, bitmap *bmp)
{
	if(!fm.core->visible || data->viewport_size >= data->content_size) return;
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	data->viewport_offset = min(data->viewport_offset, data->content_size - data->viewport_size);
	rectangle<int32> slider_rect = data->slider_rectangle(fm);
	gd->br.switch_solid_color(alpha_color(127, 127, 127, 255));
	gd->fill_area(slider_rect, bmp);
}

bool scroll_bar_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	scroll_bar *sb = (scroll_bar *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(sb->fm.x, sb->fm.y),
		vector<int32, 2>(sb->fm.width, sb->fm.height)).hit_test(point);
}

void scroll_bar_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	scroll_bar *sb = (scroll_bar *)(fm.addr);
	sb->model.render(handleable<frame>(sb, &sb->fm), &sb->data, gd, bmp);
}

void scroll_bar_mouse_click(indefinite<frame> fm)
{
	scroll_bar *sb = (scroll_bar *)(fm.addr);
	sb->data.mouse_click(handleable<frame>(sb, &sb->fm));
}

scroll_bar::scroll_bar()
{
	fm.hit_test = scroll_bar_hit_test;
	fm.render = scroll_bar_render;
	fm.mouse_click.callbacks.push_moving(scroll_bar_mouse_click);
	data.initialize(handleable<frame>(this, &fm));
	model.initialize(handleable<frame>(this, &fm));
}
