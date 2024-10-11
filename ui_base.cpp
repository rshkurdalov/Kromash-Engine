#include "ui_base.h"

adaptive_size<float32> operator""uiabs(long double value)
{
	return adaptive_size<float32>(adaptive_size_type::absolute, float32(value));
}

adaptive_size<float32> operator""uirel(long double value)
{
	return adaptive_size<float32>(adaptive_size_type::relative, float32(value));
}

adaptive_size<float32> operator""uiauto(long double value)
{
	return adaptive_size<float32>(adaptive_size_type::autosize, float32(value));
}