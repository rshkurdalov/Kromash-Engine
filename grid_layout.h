#pragma once
#include "frame.h"
#include "scroll_bar.h"
#include "layout_model.h"

struct grid_layout_frame
{
	handleable<frame> fm;
	horizontal_align halign;
	vertical_align valign;

	grid_layout_frame() {}

	grid_layout_frame(
		handleable<frame> fm,
		horizontal_align halign,
		vertical_align valign)
		: fm(fm),
		halign(halign),
		valign(valign) {}
};

struct grid_layout_data
{
	array<adaptive_size<float32>> rows_size;
	array<adaptive_size<float32>> columns_size;
	dynamic_matrix<grid_layout_frame> frames;
	uint64 growth_row;
	uint64 growth_column;
	scroll_bar xscroll;
	scroll_bar yscroll;

	void initialize(handleable<frame> fm);
	void insert_row(uint32 insert_idx, adaptive_size<float32> size);
	void remove_row(uint32 remove_idx);
	void insert_column(uint32 insert_idx, adaptive_size<float32> size);
	void remove_column(uint32 remove_idx);
	void subframes(handleable<frame> fm, array<handleable<frame>> *frames_addr);
	vector<uint32, 2> content_size(handleable<frame> fm, uint32 viewport_width, uint32 viewport_height);
	void update_layout(handleable<frame> fm);
	void render(handleable<frame> fm, bitmap_processor *bp, bitmap *bmp);
	void mouse_wheel_rotate(handleable<frame> fm);
};

struct grid_layout
{
	frame fm;
	grid_layout_data data;
	layout_model model;

	grid_layout();
};
