#include "graphics.h"

bitmap::bitmap()
{
	data = nullptr;
}

bitmap::~bitmap()
{
	if(data != nullptr) delete[] data;
}


void bitmap::resize(uint32 width_value, uint32 height_value)
{
	width = width_value;
	height = height_value;
	if(data != nullptr)
	{
		delete[] data;
		data = nullptr;
	}
	if(width != 0 && height != 0)
		data = new alpha_color[width * height];
}

void brush::switch_solid_color(alpha_color color_value)
{
	type = brush_type::solid;
	color = color_value;
}

void brush::switch_linear_gradient(
	gradient_stop *gradient_collection,
	uint64 size,
	vector<float32, 2> begin,
	vector<float32, 2> end)
{
	type = brush_type::linear_gradient;
	gradients.reset();
	gradients.insert_range(0, gradient_collection, gradient_collection + size);
	v1 = begin;
	v2 = end;
}

void brush::switch_radial_gradient(
	gradient_stop *gradient_collection,
	uint64 size,
	vector<float32, 2> center,
	vector<float32, 2> offset,
	float32 rx_value,
	float32 ry_value)
{
	type = brush_type::radial_gradient;
	gradients.reset();
	gradients.insert_range(0, gradient_collection, gradient_collection + size);
	v1 = center;
	v2 = offset;
	rx = rx_value;
	ry = ry_value;
}

void brush::switch_bitmap(bitmap *source_bitmap, matrix<float32, 3, 3> bitmap_transform_matrix)
{
	type = brush_type::bitmap;
	bmp = source_bitmap;
	bitmap_transform = bitmap_transform_matrix;
	reverse_transform = bitmap_transform;
	invert_matrix(&reverse_transform);
}

graphics_displayer::graphics_displayer()
{
	rasterization = rasterization_mode::fill;
	line_width = 1.0f;
	opacity = 1.0f;
	color_interpolation = color_interpolation_mode::flat;
	br.switch_solid_color(alpha_color(0, 0, 0, 255));
}

void graphics_displayer::push_transform(matrix<float32, 3, 3> transform)
{
	transform_stack.push(transform);
}

void graphics_displayer::pop_transform()
{
	transform_stack.pop();
}

void graphics_displayer::push_scissor(rectangle<int32> rect)
{
	if(scissor_stack.size == 0)
		scissor_stack.push(rect);
	else
	{
		rectangle<int32> target_rect;
		target_rect.position.x = max(
			rect.position.x,
			scissor_stack[scissor_stack.size - 1].position.x);
		target_rect.position.y = max(
			rect.position.y,
			scissor_stack[scissor_stack.size - 1].position.y);
		target_rect.extent.x = min(
			rect.position.x + rect.extent.x,
			scissor_stack[scissor_stack.size - 1].position.x
			+ scissor_stack[scissor_stack.size - 1].extent.x)
			- target_rect.position.x;
		target_rect.extent.y = min(
			rect.position.y + rect.extent.y,
			scissor_stack[scissor_stack.size - 1].position.y
			+ scissor_stack[scissor_stack.size - 1].extent.y)
			- target_rect.position.y;
		scissor_stack.push(target_rect);
	}
}

void graphics_displayer::pop_scissor()
{
	scissor_stack.pop();
}

alpha_color graphics_displayer::point_color(uint32 x, uint32 y)
{
	if(br.type == brush_type::solid)
	{
		return br.color;
	}
	else if(br.type == brush_type::bitmap)
	{
		vector<float32, 3> mp = vector<float32, 3>(float32(x) + 0.5f, float32(y) + 0.5f, 1.0f) * br.reverse_transform;
		vector<float32, 2> p = vector<float32, 2>(mp.x, mp.y);
		int32 bx = int32(round(p.x)), by = int32(round(p.y));
		if(bx < 0 || bx >= int32(br.bmp->width) || by < 0 || by >= int32(br.bmp->height))
			return alpha_color(0, 0, 0, 0);
		return br.bmp->data[(br.bmp->height - 1 - uint32(by)) * br.bmp->width + uint32(bx)];
	}
	else
	{
		if(br.gradients.size == 0) return alpha_color(0, 0, 0, 0);
		vector<float32, 2> sample = vector<float32, 2>(float32(x) + 0.5f, float32(y) + 0.5f);
		float32 grad;
		alpha_color color;
		bool hasColor = true;
		if(br.type == brush_type::linear_gradient)
		{
			vector<float32, 2> dir = vector_normal(br.v2 - br.v1);
			float32 a = vector_dot(sample - br.v1, dir);
			grad = vector_length(br.v1 + a * dir - br.v1) / vector_length(br.v2 - br.v1);
			if(a < 0.0f) grad = -grad;
		}
		else
		{
			float32 a, b, c, d, k;
			vector<float32, 2> p1, p2;
			sample -= br.v1;
			if(abs(sample.x - br.v2.x) < 0.1f)
			{
				a = br.ry * br.ry * (1.0f - br.v2.x * br.v2.x / (br.rx * br.rx));
				if(a < 0.0f) hasColor = false;
				else
				{
					a = sqrt(a);
					p1.x = br.v2.x;
					p1.y = a;
					p2.x = br.v2.x;
					p2.y = -a;
					a = (sample.y - br.v2.y) * (p1.y - br.v2.y);
					b = (sample.y - br.v2.y) * (p2.y - br.v2.y);
					if(a < 0.0f && b < 0.0f) hasColor = false;
					else
					{
						if(a < 0.0f || b >= 0.0f && a < b) swap(&p1, &p2);
						grad = (sample.y - br.v2.y) / (p1.y - br.v2.y);
					}
				}
			}
			else
			{
				k = (br.v2.y - sample.y) / (br.v2.x - sample.x);
				a = k * k + br.ry * br.ry / (br.rx * br.rx);
				b = k * (sample.y - k * sample.x);
				c = (sample.y - k * sample.x) * (sample.y - k * sample.x) - br.ry * br.ry;
				d = b * b - a * c;
				if(d < 0.0f) hasColor = false;
				else
				{
					d = sqrt(d);
					p1.x = (d - b) / a;
					p1.y = (sample.y - k * sample.x) + k * p1.x;
					p2.x = -(b + d) / a;
					p2.y = (sample.y - k * sample.x) + k * p2.x;
					a = vector_dot(sample - br.v2, p1 - br.v2);
					b = vector_dot(sample - br.v2, p2 - br.v2);
					if(a < 0.0f && b < 0.0f) hasColor = false;
					else
					{
						if(a < 0.0f || b >= 0.0f && a < b) swap(&p1, &p2);
						grad = vector_length(sample - br.v2) / vector_length(p1 - br.v2);
					}
				}
			}
				
		}
		if(!hasColor) return alpha_color(0, 0, 0, 0);
		if(grad < br.gradients[0].offset)
			color = br.gradients[0].color;
		else if(grad >= br.gradients[br.gradients.size - 1].offset)
			color = br.gradients[br.gradients.size - 1].color;
		else
		{
			uint32 j = 1;
			while(grad >= br.gradients[j].offset) j++;
			if(color_interpolation == color_interpolation_mode::flat)
				color = br.gradients[j - 1].color;
			else
			{
				float32 w = (grad - br.gradients[j - 1].offset)
					/ (br.gradients[j].offset - br.gradients[j - 1].offset);
				if(color_interpolation == color_interpolation_mode::smooth)
					w = w * w * (3.0f - 2.0f * w);
				color.r = uint8(round(float32(br.gradients[j - 1].color.r)) * (1.0f - w)
					+ float32(br.gradients[j].color.r) * w);
				color.g = uint8(round(float32(br.gradients[j - 1].color.g)) * (1.0f - w)
					+ float32(br.gradients[j].color.g) * w);
				color.b = uint8(round(float32(br.gradients[j - 1].color.b)) * (1.0f - w)
					+ float32(br.gradients[j].color.b) * w);
				color.a = uint8(round(float32(br.gradients[j - 1].color.a)) * (1.0f - w)
					+ float32(br.gradients[j].color.a) * w);
			}
		}
		return color;
	}
		
}

void outline_path(float32 width, geometry_path *path)
{
	if(path->data.size == 0) return;
	geometry_path outline_path;
	vector<float32, 2> p1, p2, p3, j1, j2, j3, j4, v1, v2;
	uint64 last_move = 0;
	width *= 0.5f;
	for(uint64 i = 0; i < path->data.size; i++)
	{
		if(path->data[i].type == geometry_path_unit::move)
		{
			last_move = i;
			p3 = path->data[i].p1;
		}
		else
		{
			if(i == 0 || path->data[i - 1].type == geometry_path_unit::move)
			{
				p3 = path->data[i].p1;
				v2 = vector_normal(p3 - p2);
				v2 = width * vector<float32, 2>(-v2.y, v2.x);
				j3 = p2 + v2;
				j4 = p2 - v2;
				outline_path.move(j3);
				outline_path.push_line(j4);
			}
			else
			{
				p3 = path->data[i].p1;
				v2 = vector_normal(p3 - p2);
				v2 = width * vector<float32, 2>(-v2.y, v2.x);
				if(intersect_lines(j1, p2 + v1, p2 + v2, p3 + v2, &j3) && vector_length(j3 - p2) < 3.0f * width
					&& intersect_lines(j2, p2 - v1, p2 - v2, p3 - v2, &j4) && vector_length(j4 - p2) < 3.0f * width)
				{
					outline_path.move(j3);
					outline_path.push_line(j1);
					outline_path.move(j2);
					outline_path.push_line(j4);
				}
				else
				{
					j3 = p2 + v1;
					outline_path.move(j3);
					outline_path.push_line(j1);
					j4 = p2 - v1;
					outline_path.move(j2);
					outline_path.push_line(j4);
					if((p3.x - p2.x) * (p2.x - p1.x) + (p3.y - p2.y) * (p2.y - p1.y) < 0.0f)
					{
						outline_path.push_line(j3);
						outline_path.move(j4);
						outline_path.push_line(j3);
						swap(&j3, &j4);
					}
				}
			}
		}
		p1 = p2;
		p2 = p3;
		v1 = v2;
		j1 = j3;
		j2 = j4;
		if(i == path->data.size - 1 || path->data[i + 1].type == geometry_path_unit::move)
		{
			if(i >= last_move + 4
				&& path->data[last_move].type == geometry_path_unit::move
				&& path->data[i].p1 == path->data[last_move].p1)
			{
				p3 = path->data[last_move + 1].p1;
				v2 = vector_normal(p3 - p2);
				v2 = width * vector<float32, 2>(-v2.y, v2.x);
				if(intersect_lines(j1, p2 + v1, p2 + v2, p3 + v2, &j3)
					&& intersect_lines(j2, p2 - v1, p2 - v2, p3 - v2, &j4))
				{
					outline_path.move(j3);
					outline_path.push_line(j1);
					outline_path.move(j2);
					outline_path.push_line(j4);
				}
				else
				{
					j3 = p2 + v1;
					outline_path.move(j3);
					outline_path.push_line(j1);
					j4 = p2 - v1;
					outline_path.move(j2);
					outline_path.push_line(j4);
					if((p3.x - p2.x) * (p2.x - p1.x) + (p3.y - p2.y) * (p2.y - p1.y) < 0.0f)
						swap(&j3, &j4);
				}
				outline_path.move(j4);
				outline_path.push_line(p3 - v2);
				outline_path.push_line(p3 + v2);
				outline_path.push_line(j3);

			}
			else if(path->data[i].type == geometry_path_unit::line)
			{
				outline_path.move(j2);
				outline_path.push_line(p2 - v1);
				outline_path.push_line(p2 + v1);
				outline_path.push_line(j1);
			}
		}
	}
	swap(&path->data, &outline_path.data);
}

void graphics_displayer::render(geometry_path &path, bitmap *bmp)
{
	const uint64 sublines = 4;
	const float32 dy = 0.25f;
	int32 x1, x2, b1, b2, m, xb, yb;
	uint64 idx, j, l, l1, l2, k;
	float32 lx = 1000000.0f, hx = -1000000.0f,
		ly = 1000000.0f, hy = -1000000.0f,
		x, y, a, c, t, ts, tm;
	alpha_color color_value, *color_addr;
	struct range_coordinate
	{
		float32 coord;
		bool negative_direction;

		range_coordinate() {}

		range_coordinate(float32 coord, bool negative_direction)
			: coord(coord), negative_direction(negative_direction) {}
	};
	array<array<range_coordinate>> ranges;
	array<float32> s;
	vector<float32, 2> v, v1, v2, v3;
	geometry_path transformed_path;
	matrix<float32, 3, 3> elliptic_arc_transform;
	vector<float32, 3> p;
	vector<float32, 2> p1, p2, p3, p4;
	if(path.data.size == 0) return;
	transformed_path.data.increase_capacity(2 * path.data.size);
	matrix<float32, 3, 3> transform;
	if(transform_stack.size == 0) set_identity_matrix(&transform);
	else transform = transform_stack.back();
	for(uint64 i = 0; i < path.data.size; i++)
	{
		if(path.data[i].type == geometry_path_unit::move)
		{
			p = vector<float32, 3>(path.data[i].p1.x, path.data[i].p1.y, 1.0f) * transform;
			p2 = vector<float32, 2>(p.x, p.y);
			transformed_path.move(p2);
		}
		else if(path.data[i].type == geometry_path_unit::line)
		{
			p = vector<float32, 3>(path.data[i].p1.x, path.data[i].p1.y, 1.0f) * transform;
			p2 = vector<float32, 2>(p.x, p.y);
			if(transformed_path.data.size != 0 && transformed_path.data[transformed_path.data.size - 1].p1 != p2)
				transformed_path.push_line(p2);
		}
		else if(path.data[i].type == geometry_path_unit::quadratic_arc)
		{
			p = vector<float32, 3>(path.data[i].p1.x, path.data[i].p1.y, 1.0f) * transform;
			p2 = vector<float32, 2>(p.x, p.y);
			p = vector<float32, 3>(path.data[i].p2.x, path.data[i].p2.y, 1.0f) * transform;
			p3 = vector<float32, 2>(p.x, p.y);
			ts = 1.0f / max(
				max(p1.x, p2.x, p3.x) - min(p1.x, p2.x, p3.x),
				max(p1.y, p2.y, p3.y) - min(p1.y, p2.y, p3.y));
			tm = 1.0f - ts;
			for(t = ts; t <= tm; t += ts)
			{
				p4 = quadratic_bezier_point(p1, p2, p3, t);
				if(transformed_path.data.size != 0 && transformed_path.data[transformed_path.data.size - 1].p1 != p4)
					transformed_path.push_line(p4);
			}
			if(transformed_path.data.size != 0 && transformed_path.data[transformed_path.data.size - 1].p1 != p3)
				transformed_path.push_line(p3);
			p2 = p3;
		}
		else
		{
			p = vector<float32, 3>(path.data[i].p1.x, path.data[i].p1.y, 1.0f) * transform;
			p2 = vector<float32, 2>(p.x, p.y);
			ts = 1.0f / (2.0f * 3.14f
				* sqrt(0.5f * (path.data[i].rx * path.data[i].rx + path.data[i].ry * path.data[i].ry)));
			tm = path.data[i].end_angle - ts;
			if(path.data[i].begin_angle >= path.data[i].end_angle)
				tm += 1.0f;
			elliptic_arc_transform = rotating_matrix(path.data[i].rotation, path.data[i].p2) * transform;
			for(t = path.data[i].begin_angle + ts; t <= tm; t += ts)
			{
				p4 = elliptic_arc_point(path.data[i].p2, path.data[i].rx, path.data[i].ry, t);
				p = vector<float32, 3>(p4.x, p4.y, 1.0f) * elliptic_arc_transform;
				p4 = vector<float32, 2>(p.x, p.y);
				if(transformed_path.data.size != 0 && transformed_path.data[transformed_path.data.size - 1].p1 != p4)
					transformed_path.push_line(p4);
			}
			if(transformed_path.data.size != 0 && transformed_path.data[transformed_path.data.size - 1].p1 != p2)
				transformed_path.push_line(p2);
		}
		p1 = p2;
	}
	if(rasterization == rasterization_mode::outline)
		outline_path(line_width, &transformed_path);
	transformed_path.orientation = path.orientation;
	for(idx = 0; idx < transformed_path.data.size; idx++)
	{
		lx = min(lx, transformed_path.data[idx].p1.x);
		hx = max(hx, transformed_path.data[idx].p1.x);
		ly = min(ly, transformed_path.data[idx].p1.y);
		hy = max(hy, transformed_path.data[idx].p1.y);
	}
	lx = floor(lx - 1.0f);
	x1 = int32(lx);
	hx = ceil(hx + 2.0f);
	x2 = int32(hx);
	ly = floor(ly);
	hy = ceil(hy);
	ranges.insert_default(0, uint64((hy - ly) + 1) * sublines);
	for(idx = 0; idx < transformed_path.data.size; idx++)
	{
		if(transformed_path.data[idx].type == geometry_path_unit::move)
			v1 = transformed_path.data[idx].p1;
		else
		{
			v2 = transformed_path.data[idx].p1;
			l1 = uint64((v1.y - ly) * float32(sublines));
			l2 = uint64((v2.y - ly) * float32(sublines));
			if(l2 < l1) swap(&l1, &l2);
			y = ly + float32(l1 + 1) * dy;
			a = (v2.x - v1.x) / (v2.y - v1.y);
			while(l1 < l2)
			{
				x = v1.x + a * (y - v1.y);
				for(j = 0; j < ranges[l1].size && x > ranges[l1][j].coord; j++);
				ranges[l1].insert(j, range_coordinate(x, v2.y < v1.y));
				l1++;
				y += dy;
			}
			v1 = v2;
		}
	}
	s.insert_default(0, (uint64)(x2 - x1));
	for(idx = 0; idx < s.size; idx++)
		s[idx] = 0.0f;
	for(l = 0; l < ranges.size; l += sublines)
	{
		yb = int32(ly) + int32(uint32(l) / sublines);
		if(yb < 0 || yb >= int32(bmp->height)
			|| scissor_stack.size != 0
			&& (yb < scissor_stack[scissor_stack.size - 1].position.y
				|| yb >= scissor_stack[scissor_stack.size - 1].position.y
				+ scissor_stack[scissor_stack.size - 1].extent.y)) continue;
		for(j = 0; j < sublines; j++)
		{
			m = 0;
			for(k = 0; k + 1 < ranges[l + j].size; k++)
			{
				if(ranges[l + j][k].negative_direction) m++;
				else m--;
				if(transformed_path.orientation == face_orientation::counterclockwise && m <= 0
					|| transformed_path.orientation == face_orientation::clockwise && m >= 0) continue;
				b1 = int32(floor(ranges[l + j][k].coord));
				b2 = int32(floor(ranges[l + j][k + 1].coord));
				if(b1 == b2) s[b1 - x1]
					+= (ranges[l + j][k + 1].coord - ranges[l + j][k].coord) * dy;
				else
				{
					s[b1 - x1] += (float32(b1 + 1) - ranges[l + j][k].coord) * dy;
					s[b2 - x1] += (ranges[l + j][k + 1].coord - float32(b2)) * dy;
					for(b1++; b1 < b2; b1++)
						s[b1 - x1] += dy;
				}
			}
		}
		for(idx = 0; idx < s.size; idx++)
		{
			if(s[idx] < 0.001f) continue;
			xb = x1 + int32(idx);
			if(xb < 0 || xb >= int32(bmp->width)
				|| scissor_stack.size != 0
				&& (xb < scissor_stack[scissor_stack.size - 1].position.x
				|| xb >= scissor_stack[scissor_stack.size - 1].position.x
				+ scissor_stack[scissor_stack.size - 1].extent.x))
			{
				s[idx] = 0.0f;
				continue;
			}
			a = s[idx] * opacity;
			color_value = point_color(xb, yb);
			color_addr = &bmp->data[(int32(bmp->height) - 1 - yb) * int32(bmp->width) + xb];
			if(color_value.a == 255 && abs(a - 1.0f) < 0.001f)
				*color_addr = color_value;
			else
			{
				color_value.a = uint8(round(float32(color_value.a) * a));
				color_addr->r = (uint32(color_value.a) * color_value.r + (255 - color_value.a) * color_addr->r) / 255;
				color_addr->g = (uint32(color_value.a) * color_value.g + (255 - color_value.a) * color_addr->g) / 255;
				color_addr->b = (uint32(color_value.a) * color_value.b + (255 - color_value.a) * color_addr->b) / 255;
				color_addr->a = max(color_addr->a, color_value.a);
			}
			s[idx] = 0.0f;
		}
	}
}

void graphics_displayer::fill_area(rectangle<int32> target_area, bitmap *target)
{
	vector<int32, 2> p,
		p1(max(0, target_area.position.x), max(0, target_area.position.y)),
		p2(min(target_area.position.x + target_area.extent.x, int32(target->width)),
			min(target_area.position.y + target_area.extent.y, int32(target->height)));
	if(scissor_stack.size != 0)
	{
		p1.x = max(p1.x, scissor_stack[scissor_stack.size - 1].position.x);
		p1.y = max(p1.y, scissor_stack[scissor_stack.size - 1].position.y);
		p2.x = min(p2.x, scissor_stack[scissor_stack.size - 1].position.x
			+ scissor_stack[scissor_stack.size - 1].extent.x);
		p2.y = min(p2.y, scissor_stack[scissor_stack.size - 1].position.y
			+ scissor_stack[scissor_stack.size - 1].extent.y);
	}
	uint32 o = uint32(255.0f * opacity);
	alpha_color color_value, *color_addr;
	if(br.type == brush_type::solid)
	{
		color_value = point_color(0, 0);
		color_value.a = uint8(uint32(color_value.a) * o / 255);
	}
	for(p.y = p1.y; p.y < p2.y; p.y++)
	{
		color_addr = &target->data[(int32(target->height) - 1 - p.y) * int32(target->width) + p1.x];
		for(p.x = p1.x; p.x < p2.x; p.x++, color_addr++)
		{
			if(br.type != brush_type::solid)
			{
				color_value = point_color(p.x, p.y);
				color_value.a = uint8(uint32(color_value.a) * o / 255);
			}
			if(color_value.a == 255)
				*color_addr = color_value;
			else if(color_value.a != 0)
			{
				color_addr->r = (uint32(color_value.a) * color_value.r + (255 - color_value.a) * color_addr->r) / 255;
				color_addr->g = (uint32(color_value.a) * color_value.g + (255 - color_value.a) * color_addr->g) / 255;
				color_addr->b = (uint32(color_value.a) * color_value.b + (255 - color_value.a) * color_addr->b) / 255;
				color_addr->a = max(color_addr->a, color_value.a);
			}
		}
	}
}

void graphics_displayer::fill_bitmap(bitmap &source, vector<int32, 2> target_point, bitmap *target)
{
	vector<int32, 2> p,
		p1(max(0, target_point.x), max(0, target_point.y)),
		p2(min(target_point.x + int32(source.width), int32(target->width)),
			min(target_point.y + int32(source.height), int32(target->height)));
	if(scissor_stack.size != 0)
	{
		p1.x = max(p1.x, scissor_stack[scissor_stack.size - 1].position.x);
		p1.y = max(p1.y, scissor_stack[scissor_stack.size - 1].position.y);
		p2.x = min(p2.x, scissor_stack[scissor_stack.size - 1].position.x
			+ scissor_stack[scissor_stack.size - 1].extent.x);
		p2.y = min(p2.y, scissor_stack[scissor_stack.size - 1].position.y
			+ scissor_stack[scissor_stack.size - 1].extent.y);
	}
	uint32 j, o = uint32(255.0f * opacity);
	alpha_color color_value, *color_addr, *source_addr;
	for(p.y = p1.y; p.y < p2.y; p.y++)
	{
		j = uint32(p.y - target_point.y);
		color_addr = &target->data[(int32(target->height) - 1 - p.y) * int32(target->width) + p1.x];
		source_addr = &source.data[(source.height - 1 - j) * source.width + uint32(p1.x - target_point.x)];
		for(p.x = p1.x; p.x < p2.x; p.x++, color_addr++, source_addr++)
		{
			color_value = *source_addr;
			if(o != 255)
				color_value.a = uint8(uint32(color_value.a) * o / 255);
			if(color_value.a == 255)
				*color_addr = color_value;
			else if((*source_addr).a != 0)
			{
				color_addr->r = (uint32(color_value.a) * color_value.r + (255 - color_value.a) * color_addr->r) / 255;
				color_addr->g = (uint32(color_value.a) * color_value.g + (255 - color_value.a) * color_addr->g) / 255;
				color_addr->b = (uint32(color_value.a) * color_value.b + (255 - color_value.a) * color_addr->b) / 255;
				color_addr->a = max(color_addr->a, color_value.a);
			}
		}
	}
}

void graphics_displayer::fill_opacity_bitmap(bitmap &source, vector<int32, 2> target_point, bitmap *target)
{
	vector<int32, 2> p,
		p1(max(0, target_point.x), max(0, target_point.y)),
		p2(min(target_point.x + int32(source.width), int32(target->width)),
			min(target_point.y + int32(source.height), int32(target->height)));
	if(scissor_stack.size != 0)
	{
		p1.x = max(p1.x, scissor_stack[scissor_stack.size - 1].position.x);
		p1.y = max(p1.y, scissor_stack[scissor_stack.size - 1].position.y);
		p2.x = min(p2.x, scissor_stack[scissor_stack.size - 1].position.x
			+ scissor_stack[scissor_stack.size - 1].extent.x);
		p2.y = min(p2.y, scissor_stack[scissor_stack.size - 1].position.y
			+ scissor_stack[scissor_stack.size - 1].extent.y);
	}
	uint32 j, o = uint32(255.0f * opacity);
	alpha_color color_value, *color_addr, *source_addr;
	if(br.type == brush_type::solid)
	{
		color_value = point_color(0, 0);
		o = o * uint32(color_value.a) / 255;
	}
	for(p.y = p1.y; p.y < p2.y; p.y++)
	{
		j = uint32(p.y - target_point.y);
		color_addr = &target->data[(int32(target->height) - 1 - p.y) * int32(target->width) + p1.x];
		source_addr = &source.data[(source.height - 1 - j) * source.width + uint32(p1.x - target_point.x)];
		for(p.x = p1.x; p.x < p2.x; p.x++, color_addr++, source_addr++)
		{
			if((*source_addr).a != 0)
			{
				if(br.type != brush_type::solid)
				{
					color_value = point_color(p.x, p.y);
					color_value.a = uint8(uint32(color_value.a)
						* uint32((*source_addr).a) * o / (255 * 255));
				}
				else if(o != 255) color_value.a = uint32((*source_addr).a) * o / 255;
				else color_value.a = (*source_addr).a;
				if(color_value.a == 255)
					*color_addr = color_value;
				else
				{
					color_addr->r = (uint32(color_value.a) * color_value.r + (255 - color_value.a) * color_addr->r) / 255;
					color_addr->g = (uint32(color_value.a) * color_value.g + (255 - color_value.a) * color_addr->g) / 255;
					color_addr->b = (uint32(color_value.a) * color_value.b + (255 - color_value.a) * color_addr->b) / 255;
					color_addr->a = max(color_addr->a, color_value.a);
				}
			}
		}
	}
}
