#include "heightmap.h"

heightmap::heightmap()
{
	map = nullptr;
}

heightmap::~heightmap()
{
	if(map != nullptr) delete[] map;
}

void heightmap::resize(uint64 width_value, uint64 length_value)
{
	width = width_value;
	length = length_value;
	if(map != nullptr) delete[] map;
	if(width != 1 && length != 1)
		map = new heightmap_unit[width * length];
	else map = nullptr;
}

void heightmap::generate_model(ID3D11Device *device)
{
	map_model.release_render_resources();
	map_model.vertices.clear();
	map_model.indices.clear();
	map_model.vertices.ensure_capacity(width * length);
	world_vertex v;
	v.normal = vector<float32, 3>(0.0f, 0.0f, 0.0f);
	v.point.z = start_z + 0.5f * unit_size;
	vector<float32, 2> uv(0.0f, float32(length - 1));
	for(uint64 i = 0; i < length; i++)
	{
		v.point.x = start_x + 0.5f * unit_size;
		uv.x = 0.0f;
		for(uint64 j = 0; j < width; j++)
		{
			v.point.y = map[i * width + j].height;
			map_model.vertices.push(v);
			v.point.x += unit_size;
			v.texture = uv;
			uv.x += 1.0f;
		}
		v.point.z += unit_size;
		uv.y -= 1.0f;
	}
	map_model.indices.ensure_capacity((width - 1) * (length - 1) * 6);
	vector<float32, 3> v1, v2, v3;
	for(uint64 i = 0; i < length - 1; i++)
	{
		for(uint64 j = 0; j < width - 1; j++)
		{
			map_model.indices.push(uint32(i * width + j));
			map_model.indices.push(uint32((i + 1) * width + j));
			map_model.indices.push(uint32(i * width + j + 1));
			v1 = map_model.vertices[i * width + j + 1].point - map_model.vertices[i * width + j].point;
			v2 = map_model.vertices[(i + 1) * width + j].point - map_model.vertices[i * width + j].point;
			v3 = vector_cross(v1, v2);
			map_model.vertices[i * width + j].normal += v3;
			map_model.vertices[(i + 1) * width + j].normal += v3;
			map_model.vertices[i * width + j + 1].normal += v3;
			map_model.indices.push(uint32(i * width + j + 1));
			map_model.indices.push(uint32((i + 1) * width + j));
			map_model.indices.push(uint32((i + 1) * width + j + 1));
			v1 = map_model.vertices[(i + 1) * width + j + 1].point - map_model.vertices[i * width + j + 1].point;
			v2 = map_model.vertices[(i + 1) * width + j].point - map_model.vertices[i * width + j + 1].point;
			v3 = vector_cross(v1, v2);
			map_model.vertices[i * width + j + 1].normal += v3;
			map_model.vertices[(i + 1) * width + j].normal += v3;
			map_model.vertices[(i + 1) * width + j + 1].normal += v3;
		}
	}
	for(uint64 i = 0; i < map_model.vertices.size; i++)
		map_model.vertices[i].normal = vector_normal(map_model.vertices[i].normal);
	map_model.initialize_render_resources(device);
}
