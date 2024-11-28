#include "text_field.h"
#include "timer.h"
#include "os_api.h"

bool caret_visible;
timer_trigger_postaction caret_timer_callback()
{
	caret_visible = !caret_visible;
	os_update_windows();
	return timer_trigger_postaction::reactivate;
}
timer *caret_timer = nullptr;

void text_field_data::initialize(handleable<frame> fm)
{
	font <<= u"cambria";
	font_size = 20;
	caret = 0;
	caret_trailing = false;
	position = 0;
	select_position = 0;
	selecting = false;
	tl.multiline = true;
	editable = false;
}

void text_field_data::update_position_by_mouse(handleable<frame> fm)
{
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	if(tl.multiline)
		tl.hit_test_point(
			vector<int32, 2>(
				mouse()->position.x - content_viewport.position.x,
				mouse()->position.y - (content_viewport.position.y + int32(scroll.data.viewport_offset))),
			&caret, &caret_trailing);
	else tl.hit_test_point(
			vector<int32, 2>(
				mouse()->position.x - (content_viewport.position.x - int32(scroll.data.viewport_offset)),
				mouse()->position.y - content_viewport.position.y),
			&caret, &caret_trailing);
	position = caret;
	if(caret_trailing) position++;
}

void text_field_data::select(uint64 idx_begin, uint64 idx_end)
{
	select_position = idx_begin;
	position = idx_end;
	caret = position;
	caret_trailing = false;
}

void text_field_data::deselect(uint64 idx_position)
{
	select_position = idx_position;
	position = idx_position;
	caret = position;
	caret_trailing = false;
}

void text_field_data::insert(u16string &text)
{
	remove();
	tl.insert_text(position, text, font, font_size);
	position += text.size;
	caret += text.size;
	caret = position;
	caret_trailing = false;
	select_position = position;
}

void text_field_data::remove()
{
	if(select_position < position)
		swap(&position, &select_position);
	tl.glyphs.remove_range(position, select_position);
	caret = position;
	caret_trailing = false;
	select_position = position;
}

void text_field_data::scroll_to_caret(handleable<frame> fm)
{
	if(tl.glyphs.size == 0) return;
	rectangle<int32> content_viewport = fm.core->frame_content_viewport();
	vector<int32, 2> point;
	int32 line_height;
	tl.hit_test_position(position, &point, &line_height);
	if(tl.multiline)
	{
		point.y += content_viewport.extent.y + int32(scroll.data.viewport_offset) - int32(tl.height);
		if(point.y < 0)
			scroll.data.shift(uint32(-point.y), false);
		else if(point.y + line_height >= content_viewport.extent.y)
			scroll.data.shift(point.y + line_height - content_viewport.extent.y, true);
	}
	else
	{
		point.x -= int32(scroll.data.viewport_offset);
		if(point.x < 0)
			scroll.data.shift(uint32(-point.x), true);
		else if(point.x >= content_viewport.extent.x)
			scroll.data.shift(point.x - content_viewport.extent.x, false);
	}
}

void text_field_data::subframes(handleable<frame> fm, array<handleable<frame>> *frames)
{
	frames->push(handleable<frame>(&scroll, &scroll.fm));
}

vector<uint32, 2> text_field_data::content_size(handleable<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	uint32 tl_width = tl.width;
	tl.width = viewport_width;
	vector<int32, 2> size = tl.content_size();
	tl.width = tl_width;
	return vector<uint32, 2>(uint32(size.x), uint32(size.y));
}

struct text_field_render_glyph_args
{
	frame *fm;
	text_field_data *tf;
	graphics_displayer *gd;
};

void text_field_render_glyph(
	glyph &gl,
	vector<int32, 2> tl_point,
	vector<int32, 2> point,
	int32 baseline,
	int32 line_width,
	int32 line_height,
	uint64 idx,
	void *data,
	bitmap *bmp)
{
	text_field_render_glyph_args *args = (text_field_render_glyph_args *)(data);
	rectangle<int32> content_viewport = args->fm->frame_content_viewport();
	if(point.y - baseline + line_height < content_viewport.position.y
		|| point.y - baseline > content_viewport.position.y + content_viewport.extent.y)
		return;
	rectangle<int32> rect;
	uint64 begin = args->tf->position, end = args->tf->select_position;
	if(end < begin) swap(&begin, &end);
	if(args->tf->editable && begin <= idx && idx < end)
	{
		rect.position = vector<int32, 2>(point.x, point.y - baseline);
		rect.extent = vector<int32, 2>(gl.data->advance.x, line_height);
		args->gd->br.switch_solid_color(alpha_color(0, 0, 200, 255));
		args->gd->fill_area(rect, bmp);
	}
	if(args->tf->editable && begin <= idx && idx < end)
		args->gd->br.switch_solid_color(alpha_color(255, 255, 255, 255));
	else args->gd->br.switch_solid_color(alpha_color(0, 0, 0, 255));
	args->gd->fill_opacity_bitmap(
		gl.data->bmp,
		vector<int32, 2>(
			point.x + int32(gl.data->bmp_offset.x),
			point.y + int32(gl.data->bmp_offset.y)),
		bmp);
	if(gl.underlined)
	{
		rect.position = vector<int32, 2>(point.x, point.y + gl.data->underline_offset);
		rect.extent = vector<int32, 2>(gl.data->advance.x, gl.data->underline_size);
		args->gd->fill_area(rect, bmp);
	}
	if(gl.strikedthrough)
	{
		rect.position = vector<int32, 2>(point.x, point.y + gl.data->strikethrough_offset);
		rect.extent = vector<int32, 2>(gl.data->advance.x, gl.data->strikethrough_size);
		args->gd->fill_area(rect, bmp);
	}
	if(caret_visible
		&& focused_frame().core == args->fm
		&& args->tf->editable
		&& (idx == args->tf->caret
			|| idx == args->tf->caret - 1 && args->tf->caret == args->tf->tl.glyphs.size))
	{
		rectangle<float32> caret_rect;
		geometry_path caret_path;
		caret_rect.position = vector<float32, 2>(float32(point.x), float32(point.y - baseline) + 0.1f * float32(line_height));
		if(args->tf->caret_trailing || args->tf->caret == args->tf->tl.glyphs.size)
		{
			if(args->tf->caret == args->tf->tl.glyphs.size
				&& args->tf->tl.glyphs[args->tf->caret - 1].code == '\n')
			{
				caret_rect.position.y -= line_height;
				caret_rect.position.x = tl_point.x;
				if(args->tf->tl.halign == horizontal_align::center && int32(args->tf->tl.width) >= line_width)
					caret_rect.position.x = tl_point.x + int32(args->tf->tl.width) / 2;
				else if(args->tf->tl.halign == horizontal_align::right && int32(args->tf->tl.width) >= line_width)
					caret_rect.position.x = tl_point.x + int32(args->tf->tl.width);
			}
			caret_rect.position.x += float32(gl.data->advance.x);
		}
		caret_rect.extent = vector<float32, 2>(2.0f, 0.8f * float32(line_height));
		caret_path.push_rectangle(caret_rect);
		args->gd->br.switch_solid_color(alpha_color(0, 0, 0, 255));
		args->gd->render(caret_path, bmp);
	}
}

void text_field_data::render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	rectangle viewport = fm.core->frame_viewport(),
		content_viewport = fm.core->frame_content_viewport();
	if(!fm.core->visible
		|| viewport.extent.x <= 0 || viewport.extent.y <= 0
		|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0)
		return;
	scroll.fm.width = uint32(scroll.fm.width_desc.value);
	scroll.fm.height = uint32(viewport.extent.y);
	scroll.fm.x = viewport.position.x + viewport.extent.x - scroll.fm.width;
	scroll.fm.y = viewport.position.y;
	tl.width = uint32(content_viewport.extent.x);
	if(tl.multiline)
	{
		scroll.fm.visible = true;
		scroll.data.content_size = uint32(tl.content_size().y);
		scroll.data.viewport_size = uint32(content_viewport.extent.y);
		if(scroll.data.content_size > scroll.data.viewport_size)
		{
			content_viewport.extent.x -= scroll.fm.width;
			scroll.data.viewport_offset
				= min(scroll.data.viewport_offset, scroll.data.content_size - scroll.data.viewport_size);
		}
		else
		{
			scroll.fm.visible = false;
			scroll.data.viewport_offset = 0;
		}
		if(content_viewport.extent.x <= 0) return;
	}
	else
	{
		scroll.fm.visible = false;
		scroll.data.content_size = uint32(tl.content_size().x);
		scroll.data.viewport_size = uint32(content_viewport.extent.x);
		if(scroll.data.content_size > scroll.data.viewport_size)
			scroll.data.viewport_offset
				= min(scroll.data.viewport_offset, scroll.data.content_size - scroll.data.viewport_size);
		else scroll.data.viewport_offset = 0;
	}
	tl.width = uint32(content_viewport.extent.x);
	tl.height = uint32(content_viewport.extent.y);
	gd->push_scissor(viewport);
	text_field_render_glyph_args args;
	args.fm = fm.core;
	args.tf = this;
	args.gd = gd;
	vector<int32, 2> text_point;
	if(tl.multiline)
		text_point = vector<int32, 2>(
			content_viewport.position.x,
			content_viewport.position.y
				+ content_viewport.extent.y
				- int32(tl.height)
				+ int32(scroll.data.viewport_offset));
	else text_point = vector<int32, 2>(
		content_viewport.position.x - int32(scroll.data.viewport_offset),
		content_viewport.position.y
			+ content_viewport.extent.y
			- int32(tl.height));
	tl.render(
		text_point,
		text_field_render_glyph,
		&args,
		bmp);
	gd->pop_scissor();
	if(caret_visible
		&& focused_frame().object.addr == fm.object.addr
		&& editable
		&& tl.glyphs.size == 0)
	{
		rectangle<float32> rect;
		rect.position = vector<float32, 2>(
			content_viewport.position.x,
			float32(content_viewport.position.y + content_viewport.extent.y) - 0.9f * float32(font_size));
		rect.extent = vector<float32, 2>(2.0f, 0.8f * float32(font_size));
		geometry_path rect_path;
		rect_path.push_rectangle(rect);
		gd->rasterization = rasterization_mode::fill;
		gd->br.switch_solid_color(alpha_color(0, 0, 0, 255));
		gd->render(rect_path, bmp);
	}
	scroll.fm.render(&scroll.fm, gd, bmp);
}

void text_field_data::mouse_click(handleable<frame> fm)
{
	update_position_by_mouse(fm);
	select_position = position;
	selecting = true;
}

void text_field_data::mouse_move(handleable<frame> fm)
{
	if(selecting)
	{
		if(!mouse()->left_pressed)
			selecting = false;
		else update_position_by_mouse(fm);
	}
}

void text_field_data::focus_receive(handleable<frame> fm)
{
	if(!editable) return;
	caret_visible = true;
	if(caret_timer == nullptr)
	{
		caret_timer = new timer();
		caret_timer->period = 0;
		nanoseconds period = 0;
		period << milliseconds(500);
		caret_timer->period = period.value;
		caret_timer->callback.assign(caret_timer_callback);
	}
	caret_timer->run();
}

void text_field_data::focus_loss(handleable<frame> fm)
{
	if(caret_timer != nullptr)
		caret_timer->reset();
	caret = 0;
	caret_trailing = false;
	position = 0;
	select_position = 0;
	selecting = false;
}

void text_field_data::mouse_wheel_rotate(handleable<frame> fm)
{
	scroll.data.shift(50, mouse()->wheel_forward);
}

void text_field_data::key_press(handleable<frame> fm)
{
	if(keyboard()->pressed_count == 1)
	{
		if(keyboard()->key_pressed[uint8(key_code::left)] && position != 0)
		{
			position--;
			caret = position;
			caret_trailing = false;
			select_position = position;
			scroll_to_caret(fm);
		}
		else if(keyboard()->key_pressed[uint8(key_code::right)] && position != tl.glyphs.size)
		{
			position++;
			caret = position;
			caret_trailing = false;
			select_position = position;
			scroll_to_caret(fm);
		}
		else if(keyboard()->key_pressed[uint8(key_code::down)] && tl.multiline && tl.glyphs.size != 0)
		{
			vector<int32, 2> point;
			int32 line_height;
			tl.hit_test_position(position, &point, &line_height);
			point.y--;
			tl.hit_test_point(point, &caret, &caret_trailing);
			position = caret;
			if(caret_trailing) position++;
			select_position = position;
			scroll_to_caret(fm);
		}
		else if(keyboard()->key_pressed[uint8(key_code::up)] && tl.multiline && tl.glyphs.size != 0)
		{
			vector<int32, 2> point;
			int32 line_height;
			tl.hit_test_position(position, &point, &line_height);
			point.y += line_height + 1;
			tl.hit_test_point(point, &caret, &caret_trailing);
			position = caret;
			if(caret_trailing) position++;
			select_position = position;
			scroll_to_caret(fm);
		}
	}
	else if(keyboard()->pressed_count == 2 && keyboard()->key_pressed[uint8(key_code::ctrl)])
	{
		if(keyboard()->key_pressed[uint8(key_code::c)] && position != select_position)
		{
			uint64 begin = position, end = select_position;
			if(end < begin) swap(&begin, &end);
			u16string str;
			while(begin < end)
			{
				str.push(char16(tl.glyphs[begin].code));
				begin++;
			}
			os_copy_text_to_clipboard(str);
		}
		else if(keyboard()->key_pressed[uint8(key_code::v)] && editable)
		{
			u16string str;
			os_copy_text_from_clipboard(&str);
			insert(str);
			scroll_to_caret(fm);
		}
		else if(keyboard()->key_pressed[uint8(key_code::x)] && editable)
		{
			uint64 begin = position, end = select_position;
			if(end < begin) swap(&begin, &end);
			u16string str;
			while(begin < end)
			{
				str.push(char16(tl.glyphs[begin].code));
				begin++;
			}
			os_copy_text_to_clipboard(str);
			remove();
			scroll_to_caret(fm);
		}
		else if(keyboard()->key_pressed[uint8(key_code::a)])
		{
			select(tl.glyphs.size, 0);
			scroll_to_caret(fm);
		}
	}
}

void text_field_data::char_input(handleable<frame> fm)
{
	if(!editable
		|| keyboard()->key_pressed[uint8(key_code::ctrl)]
		|| keyboard()->key_pressed[uint8(key_code::alt)])
		return;
	if(keyboard()->char_code == char32(key_code::backspace))
	{
		if(position == select_position
			&& position != 0
			&& tl.glyphs.size != 0)
			select(position - 1, position);
		remove();
		scroll_to_caret(fm);
	}
	else if(keyboard()->char_code == char32(key_code::enter)
		&& !tl.multiline
		&& focused_frame().object.addr == fm.object.addr)
		focus_frame(handleable<frame>(nullptr, nullptr));
	else if(keyboard()->char_code == char32(key_code::enter)
		|| keyboard()->char_code == char32(key_code::tab)
		|| keyboard()->char_code >= U' ')
	{
		u16string str;
		if(keyboard()->char_code == char32(key_code::enter))
			str.push(u'\n');
		else str.push(keyboard()->char_code);
		insert(str);
		scroll_to_caret(fm);
	}
}

void text_field_model::initialize(handleable<frame> fm)
{
	rendering_size = vector<uint32, 2>(0, 0);
	fm.core->padding_left = 4.0uiabs;
	fm.core->padding_bottom = 4.0uiabs;
	fm.core->padding_right = 4.0uiabs;
	fm.core->padding_top = 4.0uiabs;
}

void text_field_model::render(handleable<frame> fm, text_field_data *data, graphics_displayer *gd, bitmap *bmp)
{
	rectangle viewport = fm.core->frame_viewport(),
		content_viewport = fm.core->frame_content_viewport();
	viewport.position -= vector<int32, 2>(fm.core->x, fm.core->y);
	if(!fm.core->visible
		|| !data->editable
		|| viewport.extent.x <= 0 || viewport.extent.y <= 0
		|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0)
		return;
	if(fm.core->width != rendering_size.x || fm.core->height != rendering_size.y)
	{
		rendering_size = vector<uint32, 2>(fm.core->width, fm.core->height);
		surface.resize(fm.core->width, fm.core->height);
		for(uint32 i = 0; i < surface.width * surface.height; i++)
			surface.data[i] = alpha_color(0, 0, 0, 0);
		geometry_path path;
		path.push_rectangle(rectangle<float32>(viewport));
		graphics_displayer bp_surface;
		bp_surface.br.switch_solid_color(alpha_color(255, 255, 255, 255));
		bp_surface.render(path, &surface);
		bp_surface.rasterization = rasterization_mode::outline;
		bp_surface.line_width = 1.0f;
		bp_surface.br.switch_solid_color(alpha_color(0, 0, 0, 255));
		bp_surface.render(path, &surface);
	}
	gd->fill_bitmap(surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
}

bool text_field_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	text_field *tf = (text_field *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(tf->fm.x, tf->fm.y),
		vector<int32, 2>(tf->fm.width, tf->fm.height)).hit_test(point);
}

void text_field_subframes(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.subframes(handleable<frame>(tf, &tf->fm), frames);
}

vector<uint32, 2> text_field_content_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	text_field *tf = (text_field *)(fm.addr);
	return tf->data.content_size(handleable<frame>(tf, &tf->fm), viewport_width, viewport_height);
}

void text_field_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->model.render(handleable<frame>(tf, &tf->fm), &tf->data, gd, bmp);
	tf->data.render(handleable<frame>(tf, &tf->fm), gd, bmp);
}

void text_field_mouse_click(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.mouse_click(handleable<frame>(tf, &tf->fm));
}

void text_field_mouse_move(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.mouse_move(handleable<frame>(tf, &tf->fm));
}

void text_field_focus_receive(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.focus_receive(handleable<frame>(tf, &tf->fm));
}

void text_field_focus_loss(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.focus_loss(handleable<frame>(tf, &tf->fm));
}

void text_field_mouse_wheel_rotate(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.mouse_wheel_rotate(handleable<frame>(tf, &tf->fm));
}

void text_field_key_press(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.key_press(handleable<frame>(tf, &tf->fm));
}

void text_field_char_input(indefinite<frame> fm)
{
	text_field *tf = (text_field *)(fm.addr);
	tf->data.char_input(handleable<frame>(tf, &tf->fm));
}

text_field::text_field()
{
	fm.hit_test = text_field_hit_test;
	fm.subframes = text_field_subframes;
	fm.content_size = text_field_content_size;
	fm.render = text_field_render;
	fm.mouse_click.callbacks.push_moving(text_field_mouse_click);
	fm.mouse_move.callbacks.push_moving(text_field_mouse_move);
	fm.focus_receive.callbacks.push_moving(text_field_focus_receive);
	fm.focus_loss.callbacks.push_moving(text_field_focus_loss);
	fm.mouse_wheel_rotate.callbacks.push_moving(text_field_mouse_wheel_rotate);
	fm.key_press.callbacks.push_moving(text_field_key_press);
	fm.char_input.callbacks.push_moving(text_field_char_input);
	data.initialize(handleable<frame>(this, &fm));
	model.initialize(handleable<frame>(this, &fm));
}
