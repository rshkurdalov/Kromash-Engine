#include "frame.h"

frame::frame()
{
	width_desc = 200.0uiauto;
	min_width = 0;
	max_width = 1000000000;
	height_desc = 200.0uiauto;
	min_height = 0;
	max_height = 1000000000;
	margin_left = 2.0uiabs;
	margin_bottom = 2.0uiabs;
	margin_right = 2.0uiabs;
	margin_top = 2.0uiabs;
	padding_left =  0.05uirel;
	padding_bottom = 0.05uirel;
	padding_right = 0.05uirel;
	padding_top = 0.05uirel;
	visible = true;
	enabled = true;
	focusable = false;
	hit_test = empty_function<decltype(hit_test)>();
	subframes = empty_function<decltype(subframes)>();
	content_size = empty_function<decltype(content_size)>();
	render = empty_function<decltype(render)>();
	hook_mouse_click = false;
	return_mouse_click = false;
	hook_mouse_release = false;
	return_mouse_release = false;
	hook_mouse_move = false;
	return_mouse_move = false;
	hook_mouse_wheel_rotate = false;
	return_mouse_wheel_rotate = false;
}

rectangle<int32> frame::frame_viewport()
{
	rectangle<int32> rect;
	rect.position.x = x + int32(margin_left.resolve(float32(width)));
	rect.position.y = y + int32(margin_bottom.resolve(float32(height)));
	rect.extent.x = int32(width) - (rect.position.x - x)
		- int32(margin_right.resolve(float32(width)));
	rect.extent.y = int32(height) - (rect.position.y - y)
		- int32(margin_top.resolve(float32(height)));
	return rect;
}

rectangle<int32> frame::frame_content_viewport()
{
	rectangle<int32> rect;
	rect.position.x = x
		+ int32(margin_left.resolve(float32(width)))
		+ int32(padding_left.resolve(float32(width)));
	rect.position.y = y
		+ int32(margin_bottom.resolve(float32(height)))
		+ int32(padding_bottom.resolve(float32(height)));
	rect.extent.x = width - (rect.position.x - x)
		- int32(margin_right.resolve(float32(width)))
		- int32(padding_right.resolve(float32(width)));
	rect.extent.y = height - (rect.position.y - y)
		- int32(margin_top.resolve(float32(height)))
		- int32(padding_top.resolve(float32(height)));
	return rect;
}

vector<uint32, 2> frame::content_size_to_frame_size(uint32 content_width, uint32 content_height)
{
	vector<uint32, 2> size(content_width, content_height);
	float32 denominator = 1.0f;
	if(margin_left.type == adaptive_size_type::relative)
		denominator -= margin_left.value;
	else size.x += margin_left.value;
	if(padding_left.type == adaptive_size_type::relative)
		denominator -= padding_left.value;
	else size.x += padding_left.value;
	if(margin_right.type == adaptive_size_type::relative)
		denominator -= margin_right.value;
	else size.x += margin_right.value;
	if(padding_right.type == adaptive_size_type::relative)
		denominator -= padding_right.value;
	else size.x += padding_right.value;
	size.x = float32(size.x) / denominator;
	denominator = 1.0f;
	if(margin_bottom.type == adaptive_size_type::relative)
		denominator -= margin_bottom.value;
	else size.y += margin_bottom.value;
	if(padding_bottom.type == adaptive_size_type::relative)
		denominator -= padding_bottom.value;
	else size.y += padding_bottom.value;
	if(margin_top.type == adaptive_size_type::relative)
		denominator -= margin_top.value;
	else size.y += margin_top.value;
	if(padding_top.type == adaptive_size_type::relative)
		denominator -= padding_top.value;
	else size.y += padding_top.value;
	size.y = float32(size.y) / denominator;
	return size;
}

vector<uint32, 2> frame::frame_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	vector<uint32, 2> size, content_extent;
	size.x = uint32(width_desc.resolve(float32(viewport_width)));
	size.x = min(max(min_width, size.x), max_width);
	size.y = uint32(height_desc.resolve(float32(viewport_height)));
	size.y = min(max(min_height, size.y), max_height);
	if(width_desc.type == adaptive_size_type::autosize
		|| height_desc.type == adaptive_size_type::autosize)
	{
		if(width_desc.type == adaptive_size_type::autosize)
			viewport_width = max_width;
		else viewport_width = size.x;
		viewport_width -= uint32(margin_left.resolve(float32(viewport_width)))
			+ uint32(padding_left.resolve(float32(viewport_width)))
			+ uint32(margin_right.resolve(float32(viewport_width)))
			+ uint32(padding_right.resolve(float32(viewport_width)));
		if(height_desc.type == adaptive_size_type::autosize)
			viewport_height = max_height;
		else viewport_height = size.y;
		viewport_height -= uint32(margin_bottom.resolve(float32(viewport_height)))
			+ uint32(padding_bottom.resolve(float32(viewport_height)))
			+ uint32(margin_top.resolve(float32(viewport_height)))
			+ uint32(padding_top.resolve(float32(viewport_height)));
		content_extent = content_size(fm, viewport_width, viewport_height);
		content_extent = content_size_to_frame_size(content_extent.x, content_extent.y);
		if(width_desc.type == adaptive_size_type::autosize)
		{
			size.x = content_extent.x;
			size.x = min(max(min_width, size.x), max_width);
		}
		if(height_desc.type == adaptive_size_type::autosize)
		{
			size.y = content_extent.y;
			size.y = min(max(min_height, size.y), max_height);
		}
	}
	return size;
}

void frame::subframe_tree(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	uint64 idx_begin = frames->size, idx_end;
	subframes(fm, frames);
	idx_end = frames->size;
	while(idx_begin != idx_end)
		subframe_tree(frames->addr[idx_begin++].object, frames);
}
