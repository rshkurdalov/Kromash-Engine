#include "text_layout.h"
#include "os_api.h"

template<> struct key<glyph_data *>
{
	char32 code;
	u16string font_name;
	uint32 size;
	bool italic;
	uint32 weight;

	key(glyph_data *const &value)
	{
		code = value->code;
		font_name.insert_range(0, value->font_name.addr, value->font_name.addr + value->font_name.size);
		size = value->size;
		italic = value->italic;
		weight = value->weight;
	}

	key(char32 code, u16string &font_name_value, uint32 size, bool italic, uint32 weight)
		: code(code), size(size), italic(italic), weight(weight)
	{
		font_name << font_name_value;
	}

	bool operator<(const key &value) const
	{
		bool result = false;
		if(code < value.code) return true;
		else if(code > value.code) return false;
		if(font_name < value.font_name) return true;
		else if(font_name > value.font_name) return false;
		if(size < value.size) return true;
		else if(size > value.size) return false;
		if(italic < value.italic) return true;
		else if(italic > value.italic) return false;
		if(weight < value.weight) return true;
		else if(weight > value.weight) return false;
		return false;
	}
};

array<glyph_data *> glyph_cache;
glyph_data *unknown_glyph = nullptr;

text_layout::text_layout()
{
	width = 0;
	height = 0;
	valign = vertical_align::top;
	halign = horizontal_align::left;
	multiline = true;
}

void text_layout::insert_text(uint64 idx, u16string &str, u16string &font, uint32 font_size)
{
	glyphs.insert_default(idx, str.size);
	for(uint64 i = 0; i < str.size; i++, idx++)
	{
		glyphs[idx].code = str.addr[i];
		glyphs[idx].font_name.insert_range(0, font.addr, font.addr + font.size);
		glyphs[idx].size = font_size;
	}
}

void text_layout::update() //!!!
{
	glyph_data *data;
	for(uint64 i = 0, r; i < glyphs.size; i++)
	{
		r = glyph_cache.binary_search(key<glyph_data *>(
			glyphs[i].code,
			glyphs[i].font_name,
			glyphs[i].size,
			glyphs[i].italic,
			glyphs[i].weight));
		if(r == glyph_cache.size)
		{
			data = new glyph_data();
			data->code = glyphs[i].code;
			data->font_name = glyphs[i].font_name;
			data->size = glyphs[i].size,
			data->italic = glyphs[i].italic;
			data->weight = glyphs[i].weight;
			if(os_load_glyph(data))
			{
				glyph_cache.binary_insert(data);
				glyphs[i].data = data;
			}
			else
			{
				if(unknown_glyph == nullptr)
				{
					unknown_glyph = new glyph_data();
					unknown_glyph->size = 0;
					unknown_glyph->bmp.resize(0, 0);
					unknown_glyph->advance = vector<int32, 2>(0, 0);
					unknown_glyph->ascent = 0;
					unknown_glyph->descent = 0;
					unknown_glyph->internal_leading = 0;
					unknown_glyph->strikethrough_offset = 0;
					unknown_glyph->strikethrough_size = 0;
					unknown_glyph->underline_offset = 0;
					unknown_glyph->underline_size = 0;
				}
				delete data;
				glyphs[i].data = unknown_glyph;
			}
		}
		else glyphs[i].data = glyph_cache[r];
	}
}

vector<int32, 2> text_layout::content_size()
{
	vector<int32, 2> size(0, 0);
	int32 line_width = 0, line_height = 0;
	uint64 line_begin = 0, line_end = 0;
	update();
	for(uint64 i = 0; i <= glyphs.size; i++)
	{
		if(i == glyphs.size
			|| multiline
			&& (glyphs[i].code == U'\n'
				|| line_width + glyphs[i].data->advance.x > int32(width)))
		{
			if(i == glyphs.size)
				line_end = i;
			else if(glyphs[i].code == U'\n')
				line_end = i + 1;
			else if(glyphs[line_end].code == U' ')
			{
				while(line_end < glyphs.size && glyphs[line_end].code == U' ')
					line_end++;
			}
			else
			{
				if(line_begin != i) line_end = i;
				else line_end = i + 1;
			}
			line_width = 0;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				line_height = max(
					line_height,
					int32(glyphs[j].data->internal_leading
						+ glyphs[j].data->ascent + glyphs[j].data->descent));
				line_width += glyphs[j].data->advance.x;
			}
			size.x = max(size.x, line_width);
			size.y += line_height;
			line_width = 0;
			line_height = 0;
			line_begin = line_end;
		}
		if(i < glyphs.size)
		{
			if(glyphs[i].code == U' ')
				line_end = i;
			line_width += glyphs[i].data->advance.x;
		}
	}
	return size;
}

void text_layout::hit_test_position(uint64 idx, vector<int32, 2> *point, int32 *line_height)
{
	vector<int32, 2> p(0, int32(height)),
		text_size = content_size();
	if(valign == vertical_align::center && int32(height) >= text_size.y)
		p.y = (int32(height) + text_size.y) / 2;
	else if(valign == vertical_align::bottom && int32(height) >= text_size.y)
		p.y = text_size.y;
	int32 line_width = 0;
	uint64 line_begin = 0, line_end = 0;
	update();
	*line_height = 0;
	for(uint64 i = 0; i <= glyphs.size; i++)
	{
		if(i == glyphs.size
			|| multiline
			&& (glyphs[i].code == U'\n'
				|| line_width + glyphs[i].data->advance.x > int32(width)))
		{
			if(i == glyphs.size)
				line_end = i;
			else if(glyphs[i].code == U'\n')
				line_end = i + 1;
			else if(glyphs[line_end].code == U' ')
			{
				while(line_end < glyphs.size && glyphs[line_end].code == U' ')
					line_end++;
			}
			else
			{
				if(line_begin != i) line_end = i;
				else line_end = i + 1;
			}
			line_width = 0;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				*line_height = max(
					*line_height,
					int32(glyphs[j].data->internal_leading
						+ glyphs[j].data->ascent + glyphs[j].data->descent));
				line_width += glyphs[j].data->advance.x;
			}
			p.y -= *line_height;
			if(halign == horizontal_align::center && int32(width) >= line_width)
				p.x = (int32(width) - line_width) / 2;
			else if(halign == horizontal_align::right && int32(width) >= line_width)
				p.x = int32(width) - line_width;
			point->y = p.y;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				if(j == idx)
				{
					point->x = p.x;
					return;
				}
				p.x += glyphs[j].data->advance.x;
				if(j + 1 == idx && idx == glyphs.size)
				{
					point->x = p.x;
					return;
				}
			}
			p.x = 0;
			line_width = 0;
			*line_height = 0;
			line_begin = line_end;
		}
		if(i < glyphs.size)
		{
			if(glyphs[i].code == U' ')
				line_end = i;
			line_width += glyphs[i].data->advance.x;
		}
	}
}

void text_layout::hit_test_point(vector<int32, 2> point, uint64 *idx, bool *is_trailing)
{
	if(glyphs.size == 0)
	{
		*idx = 0;
		*is_trailing = false;
		return;
	}
	vector<int32, 2> p(0, int32(height)),
		text_size = content_size();
	if(valign == vertical_align::center && int32(height) >= text_size.y)
		p.y = (int32(height) + text_size.y) / 2;
	else if(valign == vertical_align::bottom && int32(height) >= text_size.y)
		p.y = text_size.y;
	int32 line_width = 0, line_height = 0;
	uint64 line_begin = 0, line_end = 0;
	update();
	for(uint64 i = 0; i <= glyphs.size; i++)
	{
		if(i == glyphs.size
			|| multiline
			&& (glyphs[i].code == U'\n'
				|| line_width + glyphs[i].data->advance.x > int32(width)))
		{
			if(i == glyphs.size)
				line_end = i;
			else if(glyphs[i].code == U'\n')
				line_end = i + 1;
			else if(glyphs[line_end].code == U' ')
			{
				while(line_end < glyphs.size && glyphs[line_end].code == U' ')
					line_end++;
			}
			else
			{
				if(line_begin != i) line_end = i;
				else line_end = i + 1;
			}
			line_width = 0;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				line_height = max(
					line_height,
					int32(glyphs[j].data->internal_leading
						+ glyphs[j].data->ascent
						+ glyphs[j].data->descent));
				line_width += glyphs[j].data->advance.x;
			}
			p.y -= line_height;
			if(halign == horizontal_align::center && int32(width) >= line_width)
				p.x = (int32(width) - line_width) / 2;
			else if(halign == horizontal_align::right && int32(width) >= line_width)
				p.x = int32(width) - line_width;
			if(point.y >= p.y)
			{
				for(uint64 j = line_begin; j < line_end; j++)
				{
					if(point.x < p.x + glyphs[j].data->advance.x / 2)
					{
						if(j == line_begin)
						{
							*idx = j;
							*is_trailing = false;
						}
						else
						{
							*idx = j - 1;
							*is_trailing = true;
						}
						return;
					}
					p.x += glyphs[j].data->advance.x;
				}
				*idx = line_end - 1;
				if(glyphs[*idx].code == '\n') *is_trailing = false;
				else *is_trailing = true;
				return;
			}
			p.x = 0;
			line_width = 0;
			line_height = 0;
			line_begin = line_end;
		}
		if(i < glyphs.size)
		{
			if(glyphs[i].code == U' ')
				line_end = i;
			line_width += glyphs[i].data->advance.x;
		}
	}
	*idx = glyphs.size;
	*is_trailing = false;
}

void text_layout::render(
	vector<int32, 2> point,
	void (*render_glyph_callback)(
		glyph &gl,
		vector<int32, 2> tl_point,
		vector<int32, 2> point,
		int32 baseline,
		int32 line_width,
		int32 line_height,
		uint64 idx,
		void *data,
		bitmap *bmp),
	void *data,
	bitmap *bmp)
{
	if(glyphs.size == 0) return;
	vector<int32, 2> p(point.x, point.y + int32(height)),
		text_size = content_size();
	if(valign == vertical_align::center && int32(height) >= text_size.y)
		p.y = point.y + (int32(height) + text_size.y) / 2;
	else if(valign == vertical_align::bottom && int32(height) >= text_size.y)
		p.y = point.y + text_size.y;
	int32 baseline = 0, line_width = 0, line_height = 0;
	uint64 line_begin = 0, line_end = 0;
	update();
	for(uint64 i = 0; i <= glyphs.size; i++)
	{
		if(i == glyphs.size
			|| multiline
			&& (glyphs[i].code == U'\n'
				|| line_width + glyphs[i].data->advance.x > int32(width)))
		{
			if(i == glyphs.size)
				line_end = i;
			else if(glyphs[i].code == U'\n')
				line_end = i + 1;
			else if(glyphs[line_end].code == U' ')
			{
				while(line_end < glyphs.size && glyphs[line_end].code == U' ')
					line_end++;
			}
			else
			{
				if(line_begin != i) line_end = i;
				else line_end = i + 1;
			}
			line_width = 0;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				line_height = max(
					line_height,
					int32(glyphs[j].data->internal_leading
						+ glyphs[j].data->ascent + glyphs[j].data->descent));
				baseline = max(baseline, int32(glyphs[j].data->descent));
				line_width += glyphs[j].data->advance.x;
			}
			p.y -= line_height - baseline;
			if(halign == horizontal_align::center && int32(width) >= line_width)
				p.x = point.x + (int32(width) - line_width) / 2;
			else if(halign == horizontal_align::right && int32(width) >= line_width)
				p.x = point.x + int32(width) - line_width;
			for(uint64 j = line_begin; j < line_end; j++)
			{
				render_glyph_callback(glyphs[j], point, p, baseline, line_width, line_height, j, data, bmp);
				p.x += glyphs[j].data->advance.x;
			}
			p.x = point.x;
			p.y -= baseline;
			baseline = 0;
			line_width = 0;
			line_height = 0;
			line_begin = line_end;
		}
		if(i < glyphs.size)
		{
			if(glyphs[i].code == U' ')
				line_end = i;
			line_width += glyphs[i].data->advance.x;
		}
	}
}

void text_layout_render_glyph_def(
	glyph &gl,
	vector<int32, 2> point,
	int32 baseline,
	int32 line_height,
	uint64 idx,
	void *data,
	bitmap *bmp) //!!!
{
	/*bitmap_processor bp;
	gd.transform = translate_matrix(float32(point.x), float32(point.y));
	gd.set_solid_color_brush(alpha_color(0, 0, 0, 255));
	gd.render(gl.data->path, bmp);
	rectangle<float32> rect;
	geometry_path rect_path;
	if(gl.underlined)
	{
		rect.position = vector<float32, 2>(
			0.0r,
			float32(gl.data->underline_offset));
		rect.extent = vector<float32, 2>(
			float32(gl.data->advance.x),
			float32(gl.data->underline_size));
		rect_path.push_rectangle(rect);
		gd.transform = translate_matrix(point.x, point.y);
		gd.render(rect_path, bmp);
		rect_path.data.clear();
	}
	if(gl.strikedthrough)
	{
		rect.position = vector<float32, 2>(
			0.0r,
			float32(gl.data->strikethrough_offset));
		rect.extent = vector<float32, 2>(
			float32(gl.data->advance.x),
			float32(gl.data->strikethrough_size));
		rect_path.data.clear();
		rect_path.push_rectangle(rect);
		gd.transform = translate_matrix(point.x, point.y);
		gd.render(rect_path, bmp);
		rect_path.data.clear();
	}*/
}
