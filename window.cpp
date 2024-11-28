#include "window.h"
#include "os_api.h"

handleable<frame> hovered_frame_ref;
handleable<frame> focused_frame_ref;
handleable<frame> pulled_frame_ref;

void hover_frame(handleable<frame> fm)
{
	if(hovered_frame_ref.object.addr == fm.object.addr) return;
	if(hovered_frame_ref.object.addr != nullptr)
		hovered_frame_ref.core->end_hover.trigger(hovered_frame_ref.object);
	hovered_frame_ref = fm;
	if(hovered_frame_ref.object.addr != nullptr)
		hovered_frame_ref.core->start_hover.trigger(hovered_frame_ref.object);
}

handleable<frame> hovered_frame()
{
	return hovered_frame_ref;
}

void focus_frame(handleable<frame> fm)
{
	if(focused_frame_ref.object.addr == fm.object.addr) return;
	if(focused_frame_ref.object.addr != nullptr)
		focused_frame_ref.core->focus_loss.trigger(focused_frame_ref.object);
	focused_frame_ref = fm;
	if(focused_frame_ref.object.addr != nullptr)
		focused_frame_ref.core->focus_receive.trigger(focused_frame_ref.object);
}

handleable<frame> focused_frame()
{
	return focused_frame_ref;
}

void pull_frame(handleable<frame> fm)
{
	pulled_frame_ref = fm;
}

handleable<frame> pulled_frame()
{
	return pulled_frame_ref;
}

handleable<frame> window_mouse_event_receiver(window *wnd, handleable<frame> fm, observer<indefinite<frame>> *obs)
{
	if(obs == &wnd->fm.mouse_click && fm.core->hook_mouse_click
		|| obs == &wnd->fm.mouse_release && fm.core->hook_mouse_release
		|| obs == &wnd->fm.mouse_move && fm.core->hook_mouse_move
		|| obs == &wnd->fm.mouse_wheel_rotate && fm.core->hook_mouse_wheel_rotate)
		return fm;
	handleable<frame> target = handleable<frame>(nullptr, nullptr);
	array<handleable<frame>> frames;
	fm.core->subframes(fm.object, &frames);
	for(uint64 i = 0; i < frames.size; i++)
	{
		if(frames[i].core->visible
			&& frames[i].core->hit_test(frames[i].object, mouse()->position))
			target = window_mouse_event_receiver(wnd, frames[i], obs);
	}
	if(target.object.addr == nullptr
		|| obs == &wnd->fm.mouse_click && target.core->return_mouse_click
		|| obs == &wnd->fm.mouse_release && target.core->return_mouse_release
		|| obs == &wnd->fm.mouse_move && target.core->return_mouse_move
		|| obs == &wnd->fm.mouse_wheel_rotate && target.core->return_mouse_wheel_rotate)
		return fm;
	else return target;
}

void window_mouse_click(indefinite<frame> fm)
{
	window *wnd = (window *)(fm.addr);
	handleable<frame> receiver = window_mouse_event_receiver(wnd, wnd->layout, &wnd->fm.mouse_click);
	if(receiver.core->focusable) focus_frame(receiver);
	else focus_frame(handleable<frame>(nullptr, nullptr));
	receiver.core->mouse_click.trigger(receiver.object);
}

void window_mouse_release(indefinite<frame> fm)
{
	window *wnd = (window *)(fm.addr);
	handleable<frame> receiver = window_mouse_event_receiver(wnd, wnd->layout, &wnd->fm.mouse_release);
	receiver.core->mouse_release.trigger(receiver.object);
	if(pulled_frame().object.addr != nullptr)
	{
		pulled_frame().core->mouse_release.trigger(pulled_frame().object);
		pull_frame(handleable<frame>(nullptr, nullptr));
	}
}

void window_mouse_move(indefinite<frame> fm)
{
	window *wnd = (window *)(fm.addr);
	handleable<frame> receiver = window_mouse_event_receiver(wnd, wnd->layout, &wnd->fm.mouse_move);
	hover_frame(receiver);
	receiver.core->mouse_move.trigger(receiver.object);
	if(pulled_frame().object.addr != nullptr)
		pulled_frame().core->mouse_move.trigger(pulled_frame().object);
}

void window_mouse_wheel_rotate(indefinite<frame> fm)
{
	window *wnd = (window *)(fm.addr);
	handleable<frame> receiver = window_mouse_event_receiver(wnd, wnd->layout, &wnd->fm.mouse_wheel_rotate);
	receiver.core->mouse_wheel_rotate.trigger(receiver.object);
}

void window_key_press(indefinite<frame> fm)
{
	if(focused_frame().object.addr != nullptr)
		focused_frame().core->key_press.trigger(focused_frame().object);
}

void window_key_release(indefinite<frame> fm)
{
	if(focused_frame().object.addr != nullptr)
		focused_frame().core->key_release.trigger(focused_frame().object);
}

void window_char_input(indefinite<frame> fm)
{
	if(focused_frame().object.addr != nullptr)
		focused_frame().core->char_input.trigger(focused_frame().object);
}

window::window()
{
	fm.width_desc = 600.0uiabs;
	fm.height_desc = 400.0uiabs;
	fm.width = 0;
	fm.height = 0;
	fm.mouse_click.callbacks.push_moving(window_mouse_click);
	fm.mouse_release.callbacks.push_moving(window_mouse_release);
	fm.mouse_move.callbacks.push_moving(window_mouse_move);
	fm.mouse_wheel_rotate.callbacks.push_moving(window_mouse_wheel_rotate);
	fm.key_press.callbacks.push_moving(window_key_press);
	fm.key_release.callbacks.push_moving(window_key_release);
	fm.char_input.callbacks.push_moving(window_char_input);
	os_create_window(this);
}

window::~window()
{
	os_destroy_window(this);
}

void window::update()
{
	vector<int32, 2> position = os_window_content_position(this);
	vector<uint32, 2> client_size = os_window_content_size(this),
		screen_size = os_screen_size();
	if(bmp.width != screen_size.x || bmp.height != screen_size.y)
	{
		bmp.resize(screen_size.x, screen_size.y);
		os_update_window_size(this);
		for(uint32 i = 0; i < bmp.width * bmp.height; i++)
			bmp.data[i] = alpha_color(0, 0, 0, 0);
	}
	fm.x = position.x;
	fm.y = position.y;
	fm.width = client_size.x;
	fm.height = client_size.y;
	layout.core->x = position.x;
	layout.core->y = position.y;
	layout.core->width = fm.width;
	layout.core->height = fm.height;
	set_memory(bmp.data, bmp.width * bmp.height * sizeof(alpha_color), 0);
	graphics_displayer bp;
	layout.core->render(layout.object, &bp, &bmp);
	//os_render_window(this);
}

void window::open()
{
	os_open_window(this);
}

void window::close()
{
	
}

void window::hide()
{

}
