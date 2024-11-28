#include "flow_layout.h"

void flow_layout_data::initialize(handleable<frame> fm)
{
	direction = flow_axis::x;
	offset = flow_line_offset::right;
	multiline = true;
	xscroll.data.vertical = false;
}

void flow_layout_data::subframes(handleable<frame> fm, array<handleable<frame>> *frames_addr)
{
	frames_addr->push(handleable<frame>(&xscroll, &xscroll.fm));
	frames_addr->push(handleable<frame>(&yscroll, &yscroll.fm));
	for(uint64 i = 0; i < frames.size; i++)
		frames_addr->push(frames[i].fm);
}

struct flow_layout_line_metrics
{
	uint64 frame_count;
	int32 linespace;
	int32 size1;
	int32 size2;
	int32 size3;

	flow_layout_line_metrics()
		: frame_count(0), linespace(0),
		size1(0), size2(0), size3(0) {}
};

struct flow_layout_metrics
{
	handleable<frame> fm;
	flow_layout_data *fl;
	int32 viewport_width;
	int32 viewport_height;
	int32 content_width;
	int32 content_height;
	array<flow_layout_line_metrics> line_metrics;

	flow_layout_metrics(handleable<frame> fm, flow_layout_data *fl, int32 viewport_width, int32 viewport_height)
		: fm(fm), fl(fl), viewport_width(viewport_width), viewport_height(viewport_height)
	{
		content_width = 0;
		content_height = 0;
	}
};

void flow_layout_evaluate_metrics(flow_layout_metrics *metrics)
{
	flow_layout_line_metrics line;
	horizontal_align last_halign = horizontal_align::left;
	vertical_align last_valign = vertical_align::top;
	flow_layout_frame *flf;
	uint64 end_idx = 0;
	vector<uint32, 2> size;
	if(metrics->fl->direction == flow_axis::x)
	{
		for(uint64 i = 0; i < metrics->fl->frames.size; i++)
		{
			flf = &metrics->fl->frames[i];
			size = flf->fm.core->frame_size(flf->fm.object, metrics->viewport_width, metrics->viewport_height);
			if(metrics->fl->multiline
				&& i != end_idx
				&& (flf->line_break
					|| uint32(flf->halign) < uint32(last_halign)
					|| line.size1 + line.size2 + line.size3 + (int32)size.x > metrics->viewport_width))
			{
				line.frame_count = i - end_idx;
				metrics->content_width = max(metrics->content_width, line.size1 + line.size2 + line.size3);
				metrics->content_height += line.linespace;
				metrics->line_metrics.push(line);
				end_idx = i;
				line = flow_layout_line_metrics();
			}
			last_halign = flf->halign;
			if(flf->halign == horizontal_align::left) line.size1 += int32(size.x);
			else if(flf->halign == horizontal_align::center) line.size2 += int32(size.x);
			else line.size3 += int32(size.x);
			line.linespace = max(line.linespace, int32(size.y));
		}
		if(end_idx != metrics->fl->frames.size)
		{
			line.frame_count = metrics->fl->frames.size - end_idx;
			metrics->content_width = max(metrics->content_width, line.size1 + line.size2 + line.size3);
			metrics->content_height += line.linespace;
			metrics->line_metrics.push(line);
		}
	}
	else
	{
		for(uint64 i = 0; i < metrics->fl->frames.size; i++)
		{
			flf = &metrics->fl->frames[i];
			size = flf->fm.core->frame_size(flf->fm.object, metrics->viewport_width, metrics->viewport_height);
			if(metrics->fl->multiline
				&& i != end_idx
				&& (flf->line_break
					|| uint32(flf->valign) < uint32(last_valign)
					|| line.size1 + line.size2 + line.size3 + (int32)size.y > metrics->viewport_height))
			{
				line.frame_count = i - end_idx;
				metrics->content_height = max(metrics->content_height, line.size1 + line.size2 + line.size3);
				metrics->content_width += line.linespace;
				metrics->line_metrics.push(line);
				end_idx = i;
				line = flow_layout_line_metrics();
			}
			last_valign = flf->valign;
			if(flf->valign == vertical_align::bottom) line.size1 += int32(size.y);
			else if(flf->valign == vertical_align::center) line.size2 += int32(size.y);
			else line.size3 += int32(size.y);
			line.linespace = max(line.linespace, int32(size.x));
		}
		if(end_idx != metrics->fl->frames.size)
		{
			line.frame_count = metrics->fl->frames.size - end_idx;
			metrics->content_height = max(metrics->content_height, line.size1 + line.size2 + line.size3);
			metrics->content_width += line.linespace;
			metrics->line_metrics.push(line);
		}
	}
}

vector<uint32, 2> flow_layout_data::content_size(handleable<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	flow_layout_metrics metrics(fm, this, viewport_width, viewport_height);
	flow_layout_evaluate_metrics(&metrics);
	if(metrics.content_width > int32(viewport_width))
		metrics.content_height += int32(xscroll.fm.height_desc.value);
	if(metrics.content_height > int32(viewport_height))
		metrics.content_width += int32(yscroll.fm.width_desc.value);
	return vector<uint32, 2>(metrics.content_width, metrics.content_height);
}

void flow_layout_data::update_layout(handleable<frame> fm)
{
	rectangle<int32> viewport = fm.core->frame_viewport();
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	flow_layout_metrics metrics(fm, this, content_viewport.extent.x, content_viewport.extent.y);
	flow_layout_evaluate_metrics(&metrics);
	bool reevaluate = false;
	xscroll.fm.height = uint32(xscroll.fm.height_desc.value);
	yscroll.fm.width = uint32(yscroll.fm.width_desc.value);
	if(metrics.content_width > metrics.viewport_width)
	{
		reevaluate = true;
		metrics.viewport_height -= xscroll.fm.height;
		xscroll.fm.visible = true;
	}
	else xscroll.fm.visible = false;
	if(metrics.content_height > metrics.viewport_height)
	{
		reevaluate = true;
		metrics.viewport_width -= yscroll.fm.width;
		yscroll.fm.visible = true;
	}
	else yscroll.fm.visible = false;
	if(reevaluate)
	{
		metrics.content_width = 0;
		metrics.content_height = 0;
		metrics.line_metrics.clear();
		flow_layout_evaluate_metrics(&metrics);
		xscroll.data.content_size = uint32(metrics.content_width);
		xscroll.data.viewport_size = uint32(metrics.viewport_width);
		yscroll.data.content_size = uint32(metrics.content_height);
		yscroll.data.viewport_size = uint32(metrics.viewport_height);
		xscroll.fm.width = uint32(content_viewport.extent.x);
		yscroll.fm.height = uint32(content_viewport.extent.y);
		if(xscroll.fm.visible && yscroll.fm.visible)
		{
			xscroll.fm.width -= yscroll.fm.width;
			yscroll.fm.height -= xscroll.fm.height;
		}
		xscroll.fm.x = content_viewport.position.x;
		xscroll.fm.y = content_viewport.position.y;
		yscroll.fm.x = content_viewport.position.x
			+ content_viewport.extent.x - int32(yscroll.fm.width);
		yscroll.fm.y = content_viewport.position.y
			+ content_viewport.extent.y - int32(yscroll.fm.height);
	}
	int32 offset1, offset2, offset3, fi = 0;
	vector<uint32, 2> size;
	vector<int32, 2> line_position;
	if(direction == flow_axis::x)
	{
		if(offset == flow_line_offset::right)
			line_position = vector<int32, 2>(
				content_viewport.position.x,
				content_viewport.position.y + content_viewport.extent.y);
		else line_position = vector<int32, 2>(
			content_viewport.position.x,
			content_viewport.position.y + content_viewport.extent.y - metrics.content_height);
		for(uint64 i = 0; i < metrics.line_metrics.size; i++)
		{
			if(offset == flow_line_offset::right)
				line_position.y -= metrics.line_metrics[i].linespace;
			offset1 = 0;
			offset2 = max(
				(metrics.viewport_width - metrics.line_metrics[i].size2) / 2,
				metrics.line_metrics[i].size1);
			if(metrics.line_metrics[i].size2 == 0)
				offset2 = metrics.line_metrics[i].size1;
			offset3 = max(
				metrics.viewport_width - metrics.line_metrics[i].size3,
				offset2 + metrics.line_metrics[i].size2);
			if(metrics.content_width <= metrics.viewport_width
				&& offset3 + metrics.line_metrics[i].size3 > metrics.viewport_width)
			{
				offset3 = metrics.viewport_width - metrics.line_metrics[i].size3;
				offset2 = offset3 - metrics.line_metrics[i].size2;
			}
			for(uint64 j = metrics.line_metrics[i].frame_count; j != 0; j--, fi++)
			{
				if(fm.core->height_desc.type == adaptive_size_type::autosize
					&& frames[fi].fm.core->height_desc.type == adaptive_size_type::relative)
					size = frames[fi].fm.core->frame_size(
						frames[fi].fm.object,
						metrics.viewport_width,
						metrics.line_metrics[i].linespace);
				else size = frames[fi].fm.core->frame_size(
					frames[fi].fm.object,
					metrics.viewport_width,
					metrics.viewport_height);
				frames[fi].fm.core->width = size.x;
				frames[fi].fm.core->height = size.y;
				if(frames[fi].halign == horizontal_align::left)
				{
					frames[fi].fm.core->x = line_position.x + offset1;
					offset1 += int32(frames[fi].fm.core->width);
				}
				else if(frames[fi].halign == horizontal_align::center)
				{
					frames[fi].fm.core->x = line_position.x + offset2;
					offset2 += int32(frames[fi].fm.core->width);
				}
				else
				{
					frames[fi].fm.core->x = line_position.x + offset3;
					offset3 += int32(frames[fi].fm.core->width);
				}
				if(frames[fi].valign == vertical_align::top)
					frames[fi].fm.core->y = line_position.y
						+ metrics.line_metrics[i].linespace
						- int32(frames[fi].fm.core->height);
				else if(frames[fi].valign == vertical_align::center)
					frames[fi].fm.core->y = line_position.y
						+ (metrics.line_metrics[i].linespace - int32(frames[fi].fm.core->height)) / 2;
				else frames[fi].fm.core->y = line_position.y;
			}
			if(offset == flow_line_offset::left)
				line_position.y += metrics.line_metrics[i].linespace;
		}
	}
	else
	{
		if(offset == flow_line_offset::right)
			line_position = vector<int32, 2>(
				content_viewport.position.x,
				content_viewport.position.y);
		else line_position = vector<int32, 2>(
			content_viewport.position.x + content_viewport.extent.x,
			content_viewport.position.y);
		for(uint64 i = 0; i < metrics.line_metrics.size; i++)
		{
			if(offset == flow_line_offset::left)
				line_position.x -= metrics.line_metrics[i].linespace;
			offset1 = 0;
			offset2 = max(
				(metrics.viewport_height - metrics.line_metrics[i].size2) / 2,
				metrics.line_metrics[i].size1);
			if(metrics.line_metrics[i].size2 == 0)
				offset2 = metrics.line_metrics[i].size1;
			offset3 = max(
				metrics.viewport_height - metrics.line_metrics[i].size3,
				offset2 + metrics.line_metrics[i].size2);
			if(metrics.content_height <= metrics.viewport_height
				&& offset3 + metrics.line_metrics[i].size3 > metrics.viewport_height)
			{
				offset3 = metrics.viewport_height - metrics.line_metrics[i].size3;
				offset2 = offset3 - metrics.line_metrics[i].size2;
			}
			for(uint64 j = metrics.line_metrics[i].frame_count; j != 0; j--, fi++)
			{
				if(fm.core->width_desc.type == adaptive_size_type::autosize
					&& frames[fi].fm.core->width_desc.type == adaptive_size_type::relative)
					size = frames[fi].fm.core->frame_size(
						frames[fi].fm.object,
						metrics.line_metrics[i].linespace,
						metrics.viewport_height);
				else size = frames[fi].fm.core->frame_size(
					frames[fi].fm.object,
					metrics.viewport_width,
					metrics.viewport_height);
				frames[fi].fm.core->width = size.x;
				frames[fi].fm.core->height = size.y;
				if(frames[fi].valign == vertical_align::bottom)
				{
					frames[fi].fm.core->y = line_position.y + offset1;
					offset1 += int32(frames[fi].fm.core->height);
				}
				else if(frames[fi].valign == vertical_align::center)
				{
					frames[fi].fm.core->y = line_position.y + offset2;
					offset2 += int32(frames[fi].fm.core->height);
				}
				else
				{
					frames[fi].fm.core->y = line_position.y + offset3;
					offset3 += int32(frames[fi].fm.core->height);
				}
				if(frames[fi].halign == horizontal_align::right)
					frames[fi].fm.core->x = line_position.x
						+ metrics.line_metrics[i].linespace
						- int32(frames[fi].fm.core->width);
				else if(frames[fi].halign == horizontal_align::center)
					frames[fi].fm.core->x = line_position.x
						+ (metrics.line_metrics[i].linespace - int32(frames[fi].fm.core->width)) / 2;
				else frames[fi].fm.core->x = line_position.x;
			}
			if(offset == flow_line_offset::right)
				line_position.x += metrics.line_metrics[i].linespace;
		}
	}
}

void flow_layout_data::render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	if(!fm.core->visible
		|| content_viewport.extent.x <= 0
		|| content_viewport.extent.y <= 0)
		return;
	update_layout(fm);
	gd->push_scissor(content_viewport);
	if(xscroll.data.content_size > xscroll.data.viewport_size)
		xscroll.data.viewport_offset
			= min(xscroll.data.viewport_offset, xscroll.data.content_size - xscroll.data.viewport_size);
	else xscroll.data.viewport_offset = 0;
	if(yscroll.data.content_size > yscroll.data.viewport_size)
		yscroll.data.viewport_offset
			= min(yscroll.data.viewport_offset, yscroll.data.content_size - yscroll.data.viewport_size);
	else yscroll.data.viewport_offset = 0;
	for(uint64 i = 0; i < frames.size; i++)
	{
		frames[i].fm.core->x -= int32(xscroll.data.viewport_offset);
		frames[i].fm.core->y += int32(yscroll.data.viewport_offset);
		if(frames[i].fm.core->x < content_viewport.position.x + content_viewport.extent.x
			&& frames[i].fm.core->x + int32(frames[i].fm.core->width)
				>= content_viewport.position.x
			&& frames[i].fm.core->y < content_viewport.position.y + content_viewport.extent.y
			&& frames[i].fm.core->y + int32(frames[i].fm.core->height)
				>= content_viewport.position.y)
			frames[i].fm.core->render(frames[i].fm.core, gd, bmp);
	}
	gd->pop_scissor();
	xscroll.fm.render(&xscroll.fm, gd, bmp);
	yscroll.fm.render(&yscroll.fm, gd, bmp);
}

void flow_layout_data::mouse_wheel_rotate(handleable<frame> fm)
{
	if(yscroll.fm.visible)
		yscroll.data.shift(50, mouse()->wheel_forward);
	else xscroll.data.shift(50, mouse()->wheel_forward);
}

bool flow_layout_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	flow_layout *fl = (flow_layout *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(fl->fm.x, fl->fm.y),
		vector<int32, 2>(fl->fm.width, fl->fm.height)).hit_test(point);
}

void flow_layout_subframes(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	flow_layout *fl = (flow_layout *)(fm.addr);
	fl->data.subframes(handleable<frame>(fl, &fl->fm), frames);
}

vector<uint32, 2> flow_layout_content_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	flow_layout *fl = (flow_layout *)(fm.addr);
	return fl->data.content_size(handleable<frame>(fl, &fl->fm), viewport_width, viewport_height);
}

void flow_layout_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	flow_layout *fl = (flow_layout *)(fm.addr);
	fl->model.render(handleable<frame>(fl, &fl->fm), gd, bmp);
	fl->data.render(handleable<frame>(fl, &fl->fm), gd, bmp);
}

void flow_layout_mouse_wheel_rotate(indefinite<frame> fm)
{
	flow_layout *fl = (flow_layout *)(fm.addr);
	fl->data.mouse_wheel_rotate(handleable<frame>(fl, &fl->fm));
}

flow_layout::flow_layout()
{
	fm.hit_test = flow_layout_hit_test;
	fm.subframes = flow_layout_subframes;
	fm.content_size = flow_layout_content_size;
	fm.render = flow_layout_render;
	fm.mouse_wheel_rotate.callbacks.push_moving(flow_layout_mouse_wheel_rotate);
	data.initialize(handleable<frame>(this, &fm));
	model.initialize(handleable<frame>(this, &fm));
}
