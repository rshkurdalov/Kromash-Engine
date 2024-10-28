#pragma once
#include "graphics.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

using namespace DirectX;

struct texture_view
{
	ID3D11Resource *texture;
	ID3D11ShaderResourceView *shader_resource_view;

	texture_view();
	~texture_view();
};

struct model_material
{
	array<byte> image;
	texture_view tv;
};

struct world_object
{
	array<world_vertex> vertices;
	array<uint32> indices;
	array<model_weight> weights;
	array<vector<float32, 3>> positions;
	model_material material;
	ID3D11Buffer *vertex_buffer;
	ID3D11Buffer *index_buffer;

	world_object();
	~world_object();
	void initialize_render_resources(ID3D11Device *device);
	void release_render_resources();
};

struct world_model
{
	array<world_object> subsets;
	array<model_joint> joints;
	array<model_animation> animations;

	void run_animation(ID3D11DeviceContext *device_context, uint32 animation_idx, float32 t);
};

/*Generate cylinder around y axis with radius and height of 1*/
void generate_cylinder(uint32 long_lines, array<world_vertex> *vertices);
