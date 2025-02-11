#pragma once
#include "global_operators.h"
#include "matrix.h"
#include "array.h"
#include "geometry.h"

struct alpha_color
{
	union
	{
		struct
		{
			uint8 r;
			uint8 g;
			uint8 b;
			uint8 a;
		};
		uint8 rgba[4];
	};

	alpha_color() {}
	alpha_color(uint8 r, uint8 g, uint8 b, uint8 a)
		: r(r), g(g), b(b), a(a) {}
};

struct gradient_stop
{
	float32 offset;
	alpha_color color;

	gradient_stop() {}
	gradient_stop(float32 offset, alpha_color color)
		: offset(offset), color(color) {}
};

enum struct rasterization_mode
{
	fill,
	outline
};

enum struct color_interpolation_mode
{
	flat,
	linear,
	smooth
};

struct bitmap
{
	alpha_color *data;
	uint32 width;
	uint32 height;

	bitmap();
	~bitmap();
	void resize(uint32 width_value, uint32 height_value);
};

enum struct brush_type
{
	solid,
	linear_gradient,
	radial_gradient,
	bitmap
};

struct brush
{
	brush_type type;
	alpha_color color;
	array<gradient_stop> gradients;
	vector<float32, 2> v1;
	vector<float32, 2> v2;
	float32 rx;
	float32 ry;
	bitmap *bmp;
	matrix<float32, 3, 3> bitmap_transform;
	matrix<float32, 3, 3> reverse_transform;

	void switch_solid_color(alpha_color color_value);
	void switch_linear_gradient(
		gradient_stop *gradient_collection,
		uint64 size,
		vector<float32, 2> begin,
		vector<float32, 2> end);
	void switch_radial_gradient(
		gradient_stop *gradient_collection,
		uint64 size,
		vector<float32, 2> center,
		vector<float32, 2> offset,
		float32 rx_value,
		float32 ry_value);
	void switch_bitmap(bitmap *source_bitmap, matrix<float32, 3, 3> bitmap_transform_matrix);
};

struct graphics_displayer
{
	rasterization_mode rasterization;
	float32 line_width;
	array<matrix<float32, 3, 3>> transform_stack;
	array<rectangle<int32>> scissor_stack;
	float32 opacity;
	color_interpolation_mode color_interpolation;
	brush br;

	graphics_displayer();
	void push_transform(matrix<float32, 3, 3> transform);
	void pop_transform();
	void push_scissor(rectangle<int32> rect);
	void pop_scissor();
	alpha_color point_color(uint32 x, uint32 y);
	void render(geometry_path &path, bitmap *bmp);
	void fill_area(rectangle<int32> target_area, bitmap *target);
	void fill_bitmap(bitmap &source, vector<int32, 2> target_point, bitmap *target);
	void fill_opacity_bitmap(bitmap &source, vector<int32, 2> target_point, bitmap *target);
};

struct world_vertex
{
	vector<float32, 3> point;
	vector<float32, 2> texture;
	vector<float32, 3> normal;
	vector<int32, 4> joints;
	vector<float32, 4> weights;

	world_vertex() {}

	world_vertex(vector<float32, 3> point_value) : point(point_value) {}

	world_vertex(
		vector<float32, 3> point_value,
		vector<float32, 2> texture_value)
		: point(point_value), texture(texture_value) {}

	world_vertex(
		vector<float32, 3> point_value,
		vector<float32, 2> texture_value,
		vector<float32, 3> normal_value)
		: point(point_value), texture(texture_value), normal(normal_value) {}

	world_vertex(
		vector<float32, 3> point_value,
		vector<float32, 2> texture_value,
		vector<float32, 3> normal_value,
		vector<int32, 4> joints_value,
		vector<float32, 4> weights_value)
		: point(point_value), texture(texture_value), normal(normal_value),
		joints(joints_value), weights(weights_value) {}
};
