#pragma once
#include "frame.h"
#include "scroll_bar.h"
#include "string.h"
#include "text_layout.h"

struct text_field_data
{
	text_layout tl;
	u16string font;
	uint32 font_size;
	uint64 caret;
	bool caret_trailing;
	uint64 position;
	uint64 select_position;
	bool selecting;
	bool editable;
	scroll_bar scroll;

	void initialize(handleable<frame> fm);
	void update_position_by_mouse(handleable<frame> fm);
	void select(uint64 idx_begin, uint64 idx_end);
	void deselect(uint64 idx_position);
	void insert(u16string &text);
	void remove();
	void scroll_to_caret(handleable<frame> fm);
	void subframes(handleable<frame> fm, array<handleable<frame> > *frames);
	vector<uint32, 2> content_size(handleable<frame> fm, uint32 viewport_width, uint32 viewport_height);
	void render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp);
	void mouse_click(handleable<frame> fm);
	void mouse_move(handleable<frame> fm);
	void focus_receive(handleable<frame> fm);
	void focus_loss(handleable<frame> fm);
	void mouse_wheel_rotate(handleable<frame> fm);
	void key_press(handleable<frame> fm);
	void char_input(handleable<frame> fm);
};

struct text_field_model
{
	bitmap surface;
	vector<uint32, 2> rendering_size;

	void initialize(handleable<frame> fm);
	void render(handleable<frame> fm, text_field_data *data, graphics_displayer *gd, bitmap *bmp);
};

struct text_field
{
	frame fm;
	text_field_data data;
	text_field_model model;

	text_field();
};