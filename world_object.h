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
	uint64 normal_texture;
	uint64 base_color_texture;
	uint64 metallic_texture;
	vector<float32, 4> base_color_factor;
	float32 metallic_factor;
	float32 roughness_factor;
};

struct model_node
{
	int32 parent;
	XMMATRIX offset;
	XMMATRIX scale;
	XMMATRIX rotation;
	XMMATRIX translation;
	XMMATRIX global_transform;
};

struct model_joint
{
	uint32 node;
	XMMATRIX inverse_bind_matrix;
	XMMATRIX final_transform;
};

enum struct model_channel_target
{
	scaling,
	rotating,
	translating,
	weighting
};

struct model_channel
{
	uint32 node;
	model_channel_target target;
	array<float32> timestamps;
	array<vector<float32, 4>> frames;
};

struct model_animation
{
	float32 total_animation_time;
	timestamp start_animation_time;
	array<model_channel> channels;
};

struct world_object
{
	array<world_vertex> vertices;
	array<uint32> indices;
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
	array<texture_view> textures;
	array<model_node> nodes;
	array<model_joint> joints;
	array<model_animation> animations;
	XMMATRIX root_inverse_matrix;

	void reset_animation(uint32 animation);
	void play_animation(uint32 animation);
};

/*Generate cylinder around y axis with radius and height of 1*/
void generate_cylinder(uint32 long_lines, array<world_vertex> *vertices);
