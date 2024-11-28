#include "button.h"
#include "window.h"

void push_button_data::initialize(text_field_data *tf_data)
{
	tf_data->tl.halign = horizontal_align::center;
	tf_data->tl.valign = vertical_align::center;
	tf_data->editable = false;
	button_click.assign(empty_function<void(*)()>());
}

handleable<frame> clicked_pb_frame = handleable<frame>(nullptr, nullptr);
push_button_data *clicked_pb_data = nullptr;
void button_release()
{
	if(mouse()->last_released == mouse_button::left)
	{
		if(clicked_pb_frame.core->hit_test(indefinite<frame>(clicked_pb_frame.object), mouse()->position))
			clicked_pb_data->button_click();
		mouse()->mouse_release.callbacks.remove_element(button_release);
		clicked_pb_frame = handleable<frame>(nullptr, nullptr);
		clicked_pb_data = nullptr;
	}
}

void push_button_data::mouse_click(handleable<frame> fm)
{
	if(mouse()->last_clicked == mouse_button::left)
	{
		clicked_pb_frame = fm;
		clicked_pb_data = this;
		mouse()->mouse_release.callbacks.push(button_release);
	}
}

void push_button_model::initialize(handleable<frame> fm)
{
	rendering_size = vector<uint32, 2>(0, 0);
}

void push_button_model::render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	rectangle viewport = fm.core->frame_viewport(),
		content_viewport = fm.core->frame_content_viewport();
	viewport.position -= vector<int32, 2>(fm.core->x, fm.core->y);
	if(!fm.core->visible
		|| viewport.extent.x <= 0 || viewport.extent.y <= 0
		|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0)
		return;
	if(fm.core->width != rendering_size.x || fm.core->height != rendering_size.y)
	{
		rendering_size = vector<uint32, 2>(fm.core->width, fm.core->height);
		inner_surface.resize(fm.core->width, fm.core->height);
		border_surface.resize(fm.core->width, fm.core->height);
		for(uint32 i = 0; i < fm.core->width * fm.core->height; i++)
		{
			inner_surface.data[i] = alpha_color(0, 0, 0, 0);
			border_surface.data[i] = alpha_color(0, 0, 0, 0);
		}
		geometry_path path;
		rounded_rectangle<float32> rrect(
			rectangle<float32>(viewport),
			0.07f * float32(viewport.extent.x),
			0.07f * float32(viewport.extent.y));
		path.push_rounded_rectangle(rrect);
		graphics_displayer gd_surface;
		gd_surface.br.switch_solid_color(alpha_color(0, 0, 0, 255));
		gd_surface.render(path, &inner_surface);
		gd_surface.rasterization = rasterization_mode::outline;
		gd_surface.line_width = 2.0f;
		gd_surface.br.switch_solid_color(alpha_color(100, 100, 100, 255));
		gd_surface.render(path, &border_surface);
	}
	if(clicked_pb_frame.object.addr == fm.object.addr)
		gd->br.switch_solid_color(alpha_color(170, 170, 170, 255));
	else if(hovered_frame().object.addr == fm.object.addr)
		gd->br.switch_solid_color(alpha_color(185, 185, 185, 255));
	else gd->br.switch_solid_color(alpha_color(210, 210, 210, 255));
	gd->fill_opacity_bitmap(inner_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
	if(clicked_pb_frame.object.addr == fm.object.addr || hovered_frame().object.addr == fm.object.addr)
	{
		gradient_stop stops[2];
		stops[0].offset = 0.0f;
		stops[0].color = alpha_color(140, 140, 140, 255);
		stops[1].offset = 1.0f;
		stops[1].color = alpha_color(140, 140, 140, 0);
		vector<float32, 2> position = vector<float32, 2>(mouse()->position);
		viewport.position += vector<int32, 2>(fm.core->x, fm.core->y);
		position.x = max(position.x, float32(viewport.position.x));
		position.x = min(position.x, float32(viewport.position.x + viewport.extent.x));
		position.y = max(position.y, float32(viewport.position.y));
		position.y = min(position.y, float32(viewport.position.y + viewport.extent.y));
		float32 r = float32(max(viewport.extent.x, viewport.extent.y) / 2);
		gd->br.switch_radial_gradient(stops, array_size(stops), position, vector<float32, 2>(0.0f, 0.0f), r, r);
		gd->color_interpolation = color_interpolation_mode::linear;
		gd->fill_opacity_bitmap(inner_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
	}
	gd->fill_bitmap(border_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
}

bool push_button_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	push_button *pb = (push_button *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(pb->fm.x, pb->fm.y),
		vector<int32, 2>(pb->fm.width, pb->fm.height)).hit_test(point);
}

void push_button_subframes(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.subframes(handleable<frame>(pb, &pb->fm), frames);
}

vector<uint32, 2> push_button_content_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	push_button *pb = (push_button *)(fm.addr);
	return pb->tf_data.content_size(handleable<frame>(pb, &pb->fm), viewport_width, viewport_height);
}

void push_button_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->model.render(handleable<frame>(pb, &pb->fm), gd, bmp);
	pb->tf_data.render(handleable<frame>(pb, &pb->fm), gd, bmp);
}

void push_button_mouse_click(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.mouse_click(handleable<frame>(pb, &pb->fm));
	pb->pb_data.mouse_click(handleable<frame>(pb, &pb->fm));
}

void push_button_mouse_move(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.mouse_move(handleable<frame>(pb, &pb->fm));
}

void push_button_focus_receive(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.focus_receive(handleable<frame>(pb, &pb->fm));
}

void push_button_focus_loss(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.focus_loss(handleable<frame>(pb, &pb->fm));
}

void push_button_mouse_wheel_rotate(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.mouse_wheel_rotate(handleable<frame>(pb, &pb->fm));
}

void push_button_key_press(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.key_press(handleable<frame>(pb, &pb->fm));
}

void push_button_char_input(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.char_input(handleable<frame>(pb, &pb->fm));
}

push_button::push_button()
{
	fm.hit_test = push_button_hit_test;
	fm.subframes = push_button_subframes;
	fm.content_size = push_button_content_size;
	fm.render = push_button_render;
	fm.mouse_click.callbacks.push_moving(push_button_mouse_click);
	fm.mouse_move.callbacks.push_moving(push_button_mouse_move);
	fm.focus_receive.callbacks.push_moving(push_button_focus_receive);
	fm.focus_loss.callbacks.push_moving(push_button_focus_loss);
	fm.mouse_wheel_rotate.callbacks.push_moving(push_button_mouse_wheel_rotate);
	fm.key_press.callbacks.push_moving(push_button_key_press);
	fm.char_input.callbacks.push_moving(push_button_char_input);
	tf_data.initialize(handleable<frame>(this, &fm));
	pb_data.initialize(&tf_data);
}

bool layout_button_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	layout_button *lb = (layout_button *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(lb->fm.x, lb->fm.y),
		vector<int32, 2>(lb->fm.width, lb->fm.height)).hit_test(point);
}

void layout_button_subframes(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	layout_button *lb = (layout_button *)(fm.addr);
	frames->push(handleable<frame>(&lb->button, &lb->button.fm));
	frames->push(handleable<frame>(&lb->layout, &lb->layout.fm));
}

vector<uint32, 2> layout_button_content_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height)
{
	layout_button *lb = (layout_button *)(fm.addr);
	return lb->layout.fm.content_size(indefinite<frame>(&lb->layout), viewport_width, viewport_height);
}

void layout_button_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	layout_button *lb = (layout_button *)(fm.addr);
	rectangle<int32> content_viewport = lb->fm.frame_content_viewport();
	if(!lb->fm.visible|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0) return;
	handleable<frame> f = hovered_frame();
	if(clicked_pb_frame.object.addr == &lb->button)
	{
		gd->br.switch_solid_color(alpha_color(75, 175, 255, 255));
		gd->fill_area(content_viewport, bmp);
	}
	else if(hovered_frame().object.addr == lb)
	{
		gd->br.switch_solid_color(alpha_color(50, 150, 230, 255));
		gd->fill_area(content_viewport, bmp);
	}
	lb->button.fm.x = lb->fm.x;
	lb->button.fm.y = lb->fm.y;
	lb->button.fm.width = lb->fm.width;
	lb->button.fm.height = lb->fm.height;
	lb->layout.fm.x = lb->fm.x;
	lb->layout.fm.y = lb->fm.y;
	lb->layout.fm.width = lb->fm.width;
	lb->layout.fm.height = lb->fm.height;
	lb->layout.fm.render(indefinite<frame>(&lb->layout), gd, bmp);
}

void layout_button_mouse_click(indefinite<frame> fm)
{
	layout_button *lb = (layout_button *)(fm.addr);
	lb->button.fm.mouse_click.trigger(indefinite<frame>(&lb->button));
}

void layout_button_mouse_move(indefinite<frame> fm)
{
	layout_button *lb = (layout_button *)(fm.addr);
	lb->button.fm.mouse_move.trigger(indefinite<frame>(&lb->button));
}

void layout_button_mouse_wheel_rotate(indefinite<frame> fm)
{
	layout_button *lb = (layout_button *)(fm.addr);
	lb->button.fm.mouse_wheel_rotate.trigger(indefinite<frame>(&lb->button));
}

layout_button::layout_button()
{
	fm.hit_test = layout_button_hit_test;
	fm.subframes = layout_button_subframes;
	fm.content_size = layout_button_content_size;
	fm.render = layout_button_render;
	fm.mouse_click.callbacks.push_moving(layout_button_mouse_click);
	fm.mouse_move.callbacks.push_moving(layout_button_mouse_move);
	fm.mouse_wheel_rotate.callbacks.push_moving(layout_button_mouse_wheel_rotate);
	fm.hook_mouse_move = true;
	fm.hook_mouse_click = true;
	fm.hook_mouse_release = true;
	fm.hook_mouse_wheel_rotate = true;
}

radio_button_group::radio_button_group()
{
	picked = indefinite<frame>(nullptr);
}

void radio_button_model::initialize(handleable<frame> fm)
{
	flag_size = 18;
}

void radio_button_model::render(handleable<frame> fm, radio_button_data *rb_data, graphics_displayer *gd, bitmap *bmp)
{
	rectangle viewport = fm.core->frame_viewport(),
		content_viewport = fm.core->frame_content_viewport();
	if(!fm.core->visible
		|| viewport.extent.x <= 0 || viewport.extent.y <= 0
		|| content_viewport.extent.x <= 0 || content_viewport.extent.y <= 0)
		return;
	if(flag_size != rendering_size.x || flag_size != rendering_size.y
		|| cached_flag != (rb_data->group->picked.addr == fm.object.addr))
	{
		cached_flag = rb_data->group->picked.addr == fm.object.addr;
		rendering_size = vector<uint32, 2>(flag_size, flag_size);
		surface.resize(flag_size, flag_size);
		for(uint32 i = 0; i < surface.width * surface.height; i++)
			surface.data[i] = alpha_color(0, 0, 0, 0);
		geometry_path path;
		path.move(vector<float32, 2>(0.5f * float32(flag_size), 0.0f));
		path.push_elliptic_arc(vector<float32, 2>(0.5f * float32(flag_size), 0.0f), 1.0f, 0.0f, 0.0f, 0.0f);
		graphics_displayer gd_surface;
		gd_surface.br.switch_solid_color(alpha_color(0, 0, 0, 255));
		gd_surface.rasterization = rasterization_mode::outline;
		gd_surface.render(path, &surface);
		if(cached_flag)
		{
			path.data.clear();
			path.move(vector<float32, 2>(0.5f * float32(flag_size), 0.25f * float32(flag_size)));
			path.push_elliptic_arc(vector<float32, 2>(0.5f * float32(flag_size), 0.25f * float32(flag_size)), 1.0f, 0.0f, 0.0f, 0.0f);
			gd_surface.rasterization = rasterization_mode::fill;
			gd_surface.render(path, &surface);
		}
	}

}
