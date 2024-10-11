#include "push_button.h"
#include "window.h"

void push_button_data::initialize(text_field_data *tf_data)
{
	tf_data->tl.halign = horizontal_align::center;
	tf_data->editable = false;
	button_click = nullptr;
}

void push_button_data::mouse_click(handleable<frame> fm)
{
	if(mouse()->last_clicked == mouse_button::left)
		pull_frame(fm);
}

void push_button_data::mouse_release(handleable<frame> fm)
{
	if(pulled_frame().object.addr == fm.object.addr
		&& fm.core->hit_test(fm.object, mouse()->position)
		&& button_click != nullptr)
		button_click(data);
}

void push_button_model::initialize(handleable<frame> fm)
{
	rendering_size = vector<uint32, 2>(0, 0);
}

void push_button_model::render(handleable<frame> fm, bitmap_processor *bp, bitmap *bmp)
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
		bitmap_processor bp_surface;
		bp_surface.br.switch_solid_color(alpha_color(0, 0, 0, 255));
		bp_surface.render(path, &inner_surface);
		bp_surface.rasterization = rasterization_mode::outline;
		bp_surface.line_width = 2.0f;
		bp_surface.br.switch_solid_color(alpha_color(100, 100, 100, 255));
		bp_surface.render(path, &border_surface);
	}
	if(pulled_frame().object.addr == fm.object.addr)
		bp->br.switch_solid_color(alpha_color(170, 170, 170, 255));
	else if(hovered_frame().object.addr == fm.object.addr)
		bp->br.switch_solid_color(alpha_color(185, 185, 185, 255));
	else bp->br.switch_solid_color(alpha_color(200, 200, 200, 255));
	bp->fill_opacity_bitmap(inner_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
	if(pulled_frame().object.addr == fm.object.addr || hovered_frame().object.addr == fm.object.addr)
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
		bp->br.switch_radial_gradient(stops, array_size(stops), position, vector<float32, 2>(0.0f, 0.0f), r, r);
		bp->color_interpolation = color_interpolation_mode::linear;
		bp->fill_opacity_bitmap(inner_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
	}
	bp->fill_bitmap(border_surface, vector<int32, 2>(fm.core->x, fm.core->y), bmp);
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

void push_button_render(indefinite<frame> fm, bitmap_processor *bp, bitmap *bmp)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->model.render(handleable<frame>(pb, &pb->fm), bp, bmp);
	pb->tf_data.render(handleable<frame>(pb, &pb->fm), bp, bmp);
}

void push_button_mouse_click(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->tf_data.mouse_click(handleable<frame>(pb, &pb->fm));
	pb->pb_data.mouse_click(handleable<frame>(pb, &pb->fm));
}

void push_button_mouse_release(indefinite<frame> fm)
{
	push_button *pb = (push_button *)(fm.addr);
	pb->pb_data.mouse_release(handleable<frame>(pb, &pb->fm));
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
	fm.mouse_click.callbacks.push(push_button_mouse_click);
	fm.mouse_release.callbacks.push(push_button_mouse_release);
	fm.mouse_move.callbacks.push(push_button_mouse_move);
	fm.focus_receive.callbacks.push(push_button_focus_receive);
	fm.focus_loss.callbacks.push(push_button_focus_loss);
	fm.mouse_wheel_rotate.callbacks.push(push_button_mouse_wheel_rotate);
	fm.key_press.callbacks.push(push_button_key_press);
	fm.char_input.callbacks.push(push_button_char_input);
	tf_data.initialize(handleable<frame>(this, &fm));
	pb_data.initialize(&tf_data);
}
