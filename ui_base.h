#pragma once
#include "vector.h"

enum struct adaptive_size_type
{
	absolute,
	relative,
	autosize
};

template<typename value_type> struct adaptive_size
{
	adaptive_size_type type;
	value_type value;

	adaptive_size() {}

	adaptive_size(adaptive_size_type type, value_type value)
		: type(type), value(value) {}

	value_type resolve(value_type relative_size)
	{
		if(type == adaptive_size_type::relative)
			return value * relative_size;
		else return value;
	}
};

adaptive_size<float32> operator""uiabs(long double value);
adaptive_size<float32> operator""uirel(long double value);
adaptive_size<float32> operator""uiauto(long double value);

enum struct horizontal_align
{
	left,
	center,
	right
};

enum struct vertical_align
{
	top,
	center,
	bottom
};

enum struct flow_axis
{
	x,
	y
};

enum struct flow_line_offset
{
	left,
	right
};
