#pragma once
#include "global_types.h"
#include "array.h"
#include "ui_base.h"
#include "graphics.h"
#include "hardware.h"
#include "observer.h"

struct frame
{
	int32 x;
	int32 y;
	uint32 width;
	uint32 height;
	adaptive_size<float32> width_desc;
	uint32 min_width;
	uint32 max_width;
	adaptive_size<float32> height_desc;
	uint32 min_height;
	uint32 max_height;
	adaptive_size<float32> margin_left;
	adaptive_size<float32> margin_bottom;
	adaptive_size<float32> margin_right;
	adaptive_size<float32> margin_top;
	adaptive_size<float32> padding_left;
	adaptive_size<float32> padding_bottom;
	adaptive_size<float32> padding_right;
	adaptive_size<float32> padding_top;
	bool visible;
	bool enabled;
	bool focusable;
	bool (*hit_test)(indefinite<frame> fm, vector<int32, 2> point);
	void (*subframes)(indefinite<frame> fm, array<handleable<frame>> *frames);
	vector<uint32, 2> (*content_size)(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height);
	void (*render)(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp);
	observer<indefinite<frame>> mouse_click;
	bool hook_mouse_click;
	bool return_mouse_click;
	observer<indefinite<frame>> mouse_release;
	bool hook_mouse_release;
	bool return_mouse_release;
	observer<indefinite<frame>> mouse_move;
	bool hook_mouse_move;
	bool return_mouse_move;
	observer<indefinite<frame>> mouse_wheel_rotate;
	bool hook_mouse_wheel_rotate;
	bool return_mouse_wheel_rotate;
	observer<indefinite<frame>> start_hover;
	observer<indefinite<frame>> end_hover;
	observer<indefinite<frame>> focus_receive;
	observer<indefinite<frame>> focus_loss;
	observer<indefinite<frame>> key_press;
	observer<indefinite<frame>> key_release;
	observer<indefinite<frame>> char_input;
	rectangle<int32> frame_viewport();
	rectangle<int32> frame_content_viewport();
	vector<uint32, 2> content_size_to_frame_size(uint32 content_width, uint32 content_height);
	vector<uint32, 2> frame_size(indefinite<frame> fm, uint32 viewport_width, uint32 viewport_height);
	void subframe_tree(indefinite<frame> fm, array<handleable<frame>> *frames);

	frame();
};
