#pragma once
#include "world_object.h"

struct heightmap_unit
{
	bool hole;
	float32 height;
};

struct heightmap
{
	uint64 width;
	uint64 length;
	float32 start_x;
	float32 start_z;
	float32 unit_size;
	heightmap_unit *map;
	world_object map_model;

	heightmap();
	~heightmap();
	void resize(uint64 width_value, uint64 length_value);
	void generate_model(ID3D11Device *device);
};
