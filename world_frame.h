#pragma once
#include "graphics.h"
#include "time.h"
#include "world_object.h"
#include "window.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

using namespace DirectX;

struct constant_buffer
{
	uint32 width;
	uint32 height;
	XMMATRIX wvp;
	XMMATRIX world;
	vector<float32, 4> color;
	uint32 radius;
};

struct camera_view
{
	XMVECTOR position;
	XMVECTOR target;
	XMVECTOR up;
	XMMATRIX projection;
	XMMATRIX view;
};

struct camera_movement
{
	XMVECTOR default_forward;
	XMVECTOR default_right;
	XMVECTOR camera_forward;
	XMVECTOR camera_right;
	float32 camera_yaw;
	float32 camera_pitch;
	bool move_forward;
	bool move_back;
	bool move_right;
	bool move_left;
	timestamp last_move_time;
};

struct world_object_selection
{
	bool selected;
	world_object outline;
	ID3DBlob *hit_test_vertex_shader_buffer;
	ID3D11VertexShader *hit_test_vertex_shader;
	ID3DBlob *hit_test_pixel_shader_buffer;
	ID3D11PixelShader *hit_test_pixel_shader;
	ID3DBlob *outline_vertex_shader_buffer;
	ID3D11VertexShader *outline_vertex_shader;
	ID3DBlob *outline_pixel_shader_buffer;
	ID3D11PixelShader *outline_pixel_shader;
};

struct skymap
{
	ID3D10Blob *vertex_shader_buffer;
	ID3D10Blob *pixel_shader_buffer;
	ID3D11VertexShader *vertex_shader;
	ID3D11PixelShader *pixel_shader;
	texture_view sky_tv;
	ID3D11DepthStencilState *depth_stencil;
	world_object sphere;
	XMMATRIX sphereWorld;
};

struct world
{
	window *wnd;
	uint32 width;
	uint32 height;
	IDXGISwapChain *swap_chain;
	ID3D11Device *device;
	ID3D11DeviceContext *device_context;
	ID3D11RenderTargetView *render_target_view;
	ID3D11DepthStencilView *depth_stencil_view;
	ID3D11Texture2D *depth_stencil_buffer;
	ID3D11Buffer *cbPerObjectBuffer;
	constant_buffer cb;

	ID3DBlob *vertex_shader_buffer;
	ID3D11VertexShader *vertex_shader;
	ID3DBlob *pixel_shader_buffer;
	ID3D11PixelShader *pixel_shader;
	ID3D11InputLayout *vertex_layout;
	ID3D11SamplerState *sampler_state;

	/*undeleted - begin*/
	ID3D11Texture2D *render_target;
	ID3D11RenderTargetView *rt_view;
	ID3D11ShaderResourceView *rt_shader_resource_view;
	/*undeleted - end*/

	ID3DBlob *ui_vertex_shader_buffer;
	ID3D11VertexShader *ui_vertex_shader;
	ID3DBlob *ui_pixel_shader_buffer;
	ID3D11PixelShader *ui_pixel_shader;
	ID3D11Texture2D *ui_texture;
	ID3D11ShaderResourceView *ui_srv;
	world_object uiwo;

	handleable<frame> layout;
	camera_view camera;
	camera_movement movement;
	world_object_selection selection;

	array<world_model> models;
	world_object cylinder;
	world_object cube;
	texture_view cube_tv;
	skymap sky;

	world();
	~world();
	void initialize_common_resources();
	void initialize_ui_resources();
	void initialize_outline_resources();
	void initialize_sky_resources();
	void release_common_resources();
	void release_ui_resources();
	void release_outline_resources();
	void release_sky_resources();
	void initialize_scene();
	void mouse_click();
	void update_movement();
	void resize();
	void render_sky();
	void render_objects();
	void render_outline();
	void render_ui(handleable<frame> fm, bitmap_processor *bp, bitmap *bmp);
	void render(handleable<frame> fm, bitmap_processor *bp, bitmap *bmp);
};

struct world_frame
{
	frame fm;
	world wld;

	world_frame();
};
