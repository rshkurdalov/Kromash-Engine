#include "image_view.h"

bool image_view_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	image_view *iv = (image_view *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(iv->fm.x, iv->fm.y),
		vector<int32, 2>(iv->fm.width, iv->fm.height)).hit_test(point);
}

vector<uint32, 2> image_view_content_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	image_view *iv = (image_view *)(fm.addr);
	return vector<uint32, 2>(iv->image.width, iv->image.height);
}

void image_view_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	image_view *iv = (image_view *)(fm.addr);
	rectangle<int32> content_viewport = iv->fm.frame_content_viewport();
	if(!iv->fm.visible || content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0) return;
	if(iv->image.width <= uint32(content_viewport.extent.x) && iv->image.height <= uint32(content_viewport.extent.y))
	{
		vector<int32, 2> p(
			content_viewport.position.x + (content_viewport.extent.x - int32(iv->image.width)) / 2,
			content_viewport.position.y + (content_viewport.extent.y - int32(iv->image.height)) / 2);
		gd->fill_bitmap(iv->image, p, bmp);
	}
	else
	{
		float32 scale, width, height;
		if(float32(content_viewport.extent.x) / float32(content_viewport.extent.y)
			>= float32(iv->image.width) / float32(iv->image.height))
			scale = float32(content_viewport.extent.y) / float32(iv->image.height);
		else scale = float32(content_viewport.extent.x) / float32(iv->image.width);
		width = float32(iv->image.width) * scale;
		height = float32(iv->image.height) * scale;
		vector<float32, 2> p(
			floorf(float32(content_viewport.position.x) + 0.5f * (float32(content_viewport.extent.x) - width)),
			floorf(float32(content_viewport.position.y) + 0.5f * (float32(content_viewport.extent.y) - height)));
		gd->br.switch_bitmap(&iv->image, scaling_matrix(scale, scale, vector<float32, 2>(0.0f, 0.0f)) * translating_matrix(p.x, p.y));
		gd->fill_area(rectangle<int32>(vector<int32, 2>(int32(p.x), int32(p.y)), vector<int32, 2>(int32(width), int32(height))), bmp);
	}
}

image_view::image_view()
{
	fm.hit_test = image_view_hit_test;
	fm.content_size = image_view_content_size;
	fm.render = image_view_render;
}
