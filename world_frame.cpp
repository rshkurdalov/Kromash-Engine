#include "world_frame.h"
#include "hardware.h"
#include "os_api.h"
#include "gltf.h"
#include <thread>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")

world::world()
{
	swap_chain = nullptr;
	device = nullptr;
	device_context = nullptr;
	render_target_view = nullptr;
	depth_stencil_view = nullptr;
	depth_stencil_buffer = nullptr;
	cb_handler = nullptr;
	cb_joints_handler = nullptr;
	vertex_layout = nullptr;
	sampler_state = nullptr;

	vertex_shader_buffer = nullptr;
	vertex_shader = nullptr;
	pixel_shader_buffer = nullptr;
	pixel_shader = nullptr;

	render_target = nullptr;
	rt_view = nullptr;
	rt_shader_resource_view = nullptr;
	
	ui_vertex_shader_buffer = nullptr;
	ui_vertex_shader = nullptr;
	ui_pixel_shader_buffer = nullptr;
	ui_pixel_shader = nullptr;
	ui_texture = nullptr;
	ui_srv = nullptr;

	selection.hit_test_vertex_shader_buffer = nullptr;
	selection.hit_test_vertex_shader = nullptr;
	selection.hit_test_pixel_shader_buffer = nullptr;
	selection.hit_test_pixel_shader = nullptr;
	selection.outline_vertex_shader_buffer = nullptr;
	selection.outline_vertex_shader = nullptr;
	selection.outline_pixel_shader_buffer = nullptr;
	selection.outline_pixel_shader = nullptr;

	sky.vertex_shader_buffer = nullptr;
	sky.pixel_shader_buffer = nullptr;
	sky.vertex_shader = nullptr;
	sky.pixel_shader = nullptr;
	sky.depth_stencil = nullptr;
}

world::~world()
{
	release_common_resources();
	release_ui_resources();
	release_outline_resources();
	release_sky_resources();
}

void world::initialize_common_resources()
{
	RECT rect;
	GetClientRect(HWND(wnd->handler), &rect);
	width = uint32(rect.right - rect.left);
	height = uint32(rect.bottom - rect.top);

	DXGI_MODE_DESC buffer_desc;
    ZeroMemory(&buffer_desc, sizeof(DXGI_MODE_DESC));
    buffer_desc.Width = width;
    buffer_desc.Height = height;
    buffer_desc.RefreshRate.Numerator = 60;
    buffer_desc.RefreshRate.Denominator = 1;
    buffer_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    buffer_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    buffer_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SWAP_CHAIN_DESC swap_chain_desc; 
    ZeroMemory(&swap_chain_desc, sizeof(DXGI_SWAP_CHAIN_DESC));
    swap_chain_desc.BufferDesc = buffer_desc;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 1;
    swap_chain_desc.OutputWindow = HWND(wnd->handler); 
    swap_chain_desc.Windowed = TRUE; 
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL, NULL, NULL,
        D3D11_SDK_VERSION, &swap_chain_desc, &swap_chain, &device, NULL, &device_context);

	ID3D11Texture2D *buffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)(&buffer));
    device->CreateRenderTargetView(buffer, NULL, &render_target_view);
    buffer->Release();

	D3D11_TEXTURE2D_DESC depth_stencil_desc;
    depth_stencil_desc.Width = buffer_desc.Width;
    depth_stencil_desc.Height = buffer_desc.Height;
    depth_stencil_desc.MipLevels = 1;
    depth_stencil_desc.ArraySize = 1;
    depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_desc.SampleDesc.Count = 1;
    depth_stencil_desc.SampleDesc.Quality = 0;
    depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_stencil_desc.CPUAccessFlags = 0; 
    depth_stencil_desc.MiscFlags = 0;

    device->CreateTexture2D(&depth_stencil_desc, NULL, &depth_stencil_buffer);
    device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);

	D3D11_BUFFER_DESC cbbd;    
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));
    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(constant_buffer);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;
	device->CreateBuffer(&cbbd, NULL, &cb_handler);
   
    ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));
    cbbd.Usage = D3D11_USAGE_DEFAULT;
    cbbd.ByteWidth = sizeof(constant_buffer_joints);
    cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbbd.CPUAccessFlags = 0;
    cbbd.MiscFlags = 0;
	device->CreateBuffer(&cbbd, NULL, &cb_joints_handler);

	ID3DBlob *err;
	char *str;
	D3DCompileFromFile(L"content\\shader.fx", 0, 0, "vertex_shader", "vs_5_0", 0, 0, &vertex_shader_buffer, &err);
	if(err) str = (char *)err->GetBufferPointer();
    D3DCompileFromFile(L"content\\shader.fx", 0, 0, "pixel_shader", "ps_5_0", 0, 0, &pixel_shader_buffer, 0);
    device->CreateVertexShader(vertex_shader_buffer->GetBufferPointer(), vertex_shader_buffer->GetBufferSize(), NULL, &vertex_shader);
    device->CreatePixelShader(pixel_shader_buffer->GetBufferPointer(), pixel_shader_buffer->GetBufferSize(), NULL, &pixel_shader);
	
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"JOINTS", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	UINT numElements = array_size(layout);
	device->CreateInputLayout(
		layout,
		numElements,
		vertex_shader_buffer->GetBufferPointer(), 
		vertex_shader_buffer->GetBufferSize(),
		&vertex_layout);

	D3D11_SAMPLER_DESC samp_desc;
	ZeroMemory(&samp_desc, sizeof(samp_desc));
	samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD = 0;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&samp_desc, &sampler_state);

    D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
	D3D11_TEXTURE2D_DESC texture_desc;
    ZeroMemory(&texture_desc, sizeof(texture_desc));
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;
    device->CreateTexture2D(&texture_desc, NULL, &render_target);
	
    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
    render_target_view_desc.Format = texture_desc.Format;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    render_target_view_desc.Texture2D.MipSlice = 0;
    device->CreateRenderTargetView(render_target, &render_target_view_desc, &rt_view);

	shader_resource_view_desc.Format = texture_desc.Format;
    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
    shader_resource_view_desc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(render_target, &shader_resource_view_desc, &rt_shader_resource_view);
}

void world::initialize_ui_resources()
{
	D3DCompileFromFile(L"content\\shader.fx", 0, 0, "ui_vertex_shader", "vs_5_0", 0, 0, &ui_vertex_shader_buffer, 0);
    D3DCompileFromFile(L"content\\shader.fx", 0, 0, "ui_pixel_shader", "ps_5_0", 0, 0, &ui_pixel_shader_buffer, 0);
    device->CreateVertexShader(ui_vertex_shader_buffer->GetBufferPointer(), ui_vertex_shader_buffer->GetBufferSize(), NULL, &ui_vertex_shader);
    device->CreatePixelShader(ui_pixel_shader_buffer->GetBufferPointer(), ui_pixel_shader_buffer->GetBufferSize(), NULL, &ui_pixel_shader);

	D3D11_TEXTURE2D_DESC ui_texture_desc = CD3D11_TEXTURE2D_DESC(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		width,
		height,
		1,
		1,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_USAGE_DYNAMIC,
		D3D11_CPU_ACCESS_WRITE,
		1,
		0,
		0);
	
	device->CreateTexture2D(&ui_texture_desc, nullptr, &ui_texture);
    device->CreateShaderResourceView(ui_texture, NULL, &ui_srv);
}

void world::initialize_outline_resources()
{
	D3DCompileFromFile(L"content\\shader.fx", 0, 0, "hit_test_vertex_shader", "vs_5_0", 0, 0, &selection.hit_test_vertex_shader_buffer, 0);
    D3DCompileFromFile(L"content\\shader.fx", 0, 0, "hit_test_pixel_shader", "ps_5_0", 0, 0, &selection.hit_test_pixel_shader_buffer, 0);
    device->CreateVertexShader(
		selection.hit_test_vertex_shader_buffer->GetBufferPointer(),
		selection.hit_test_vertex_shader_buffer->GetBufferSize(),
		NULL,
		&selection.hit_test_vertex_shader);
    device->CreatePixelShader(
		selection.hit_test_pixel_shader_buffer->GetBufferPointer(),
		selection.hit_test_pixel_shader_buffer->GetBufferSize(),
		NULL,
		&selection.hit_test_pixel_shader);

	D3DCompileFromFile(L"content\\shader.fx", 0, 0, "outline_vertex_shader", "vs_5_0", 0, 0, &selection.outline_vertex_shader_buffer, 0);
    D3DCompileFromFile(L"content\\shader.fx", 0, 0, "outline_pixel_shader", "ps_5_0", 0, 0, &selection.outline_pixel_shader_buffer, 0);
    device->CreateVertexShader(
		selection.outline_vertex_shader_buffer->GetBufferPointer(),
		selection.outline_vertex_shader_buffer->GetBufferSize(),
		NULL,
		&selection.outline_vertex_shader);
    device->CreatePixelShader(
		selection.outline_pixel_shader_buffer->GetBufferPointer(),
		selection.outline_pixel_shader_buffer->GetBufferSize(),
		NULL,
		&selection.outline_pixel_shader);
}

void world::initialize_sky_resources()
{
	int32 lat_lines = 10, long_lines = 10,
		vertex_count = ((lat_lines - 2) * long_lines) + 2,
		face_count  = ((lat_lines - 3) * (long_lines) * 2) + (long_lines * 2);
    float sphereYaw = 0.0f;
    float spherePitch = 0.0f;
	sky.sphere.vertices.insert_default(0, vertex_count);

    XMVECTOR currVertPos = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    sky.sphere.vertices[0].point.x = 0.0f;
    sky.sphere.vertices[0].point.y = 0.0f;
    sky.sphere.vertices[0].point.z = 1.0f;
    for(int32 i = 0; i < lat_lines - 2; i++)
    {
        spherePitch = (i + 1) * (pi / (lat_lines - 1));
        XMMATRIX Rotationx = XMMatrixRotationX(spherePitch);
        for(DWORD j = 0; j < long_lines; j++)
        {
            sphereYaw = j * (2.0 * pi / long_lines);
            XMMATRIX Rotationy = XMMatrixRotationZ(sphereYaw);
            currVertPos = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), (Rotationx * Rotationy));    
            currVertPos = XMVector3Normalize(currVertPos);
            sky.sphere.vertices[i * long_lines + j + 1].point.x = XMVectorGetX(currVertPos);
            sky.sphere.vertices[i * long_lines + j + 1].point.y = XMVectorGetY(currVertPos);
            sky.sphere.vertices[i * long_lines + j + 1].point.z = XMVectorGetZ(currVertPos);
        }
    }
    sky.sphere.vertices[sky.sphere.vertices.size - 1].point.x = 0.0f;
    sky.sphere.vertices[sky.sphere.vertices.size - 1].point.y = 0.0f;
    sky.sphere.vertices[sky.sphere.vertices.size - 1].point.z = -1.0f;

	sky.sphere.indices.insert_default(0, face_count * 3);
    int32 k = 0;
    for(int32 l = 0; l < long_lines - 1; l++)
    {
        sky.sphere.indices[k] = 0;
        sky.sphere.indices[k + 1] = l + 1;
        sky.sphere.indices[k + 2] = l + 2;
        k += 3;
    }
    sky.sphere.indices[k] = 0;
    sky.sphere.indices[k + 1] = long_lines;
    sky.sphere.indices[k + 2] = 1;
    k += 3;

    for(int32 i = 0; i < lat_lines - 3; i++)
    {
        for(int32 j = 0; j < long_lines - 1; j++)
        {
            sky.sphere.indices[k] = i * long_lines + j + 1;
            sky.sphere.indices[k + 1] = i * long_lines + j + 2;
            sky.sphere.indices[k + 2] = (i + 1) * long_lines + j + 1;
            sky.sphere.indices[k + 3] = (i + 1) * long_lines + j + 1;
            sky.sphere.indices[k + 4] = i * long_lines + j + 2;
            sky.sphere.indices[k + 5] = (i + 1) * long_lines + j + 2;
            k += 6;
        }
        sky.sphere.indices[k] = i * long_lines + long_lines;
        sky.sphere.indices[k + 1] = i * long_lines + 1;
        sky.sphere.indices[k + 2] = (i + 1) * long_lines + long_lines;
        sky.sphere.indices[k + 3] = (i + 1) * long_lines + long_lines;
        sky.sphere.indices[k + 4] = i * long_lines + 1;
        sky.sphere.indices[k + 5] = (i + 1) * long_lines+ 1;
        k += 6;
    }
    for(int32 l = 0; l < long_lines - 1; l++)
    {
        sky.sphere.indices[k] = sky.sphere.vertices.size - 1;
        sky.sphere.indices[k + 1] = (sky.sphere.vertices.size - 1) - (l + 1);
        sky.sphere.indices[k + 2] = (sky.sphere.vertices.size - 1) - (l + 2);
        k += 3;
    }
    sky.sphere.indices[k] = sky.sphere.vertices.size - 1;
    sky.sphere.indices[k + 1] = (sky.sphere.vertices.size - 1) - long_lines;
    sky.sphere.indices[k + 2] = sky.sphere.vertices.size - 2;

	sky.sphere.initialize_render_resources(device);

	D3DCompileFromFile(L"content\\shader.fx", 0, 0, "skymap_vertex_shader", "vs_5_0", 0, 0, &sky.vertex_shader_buffer, 0);
    D3DCompileFromFile(L"content\\shader.fx", 0, 0, "skymap_pixel_shader", "ps_5_0", 0, 0, &sky.pixel_shader_buffer, 0);
    device->CreateVertexShader(sky.vertex_shader_buffer->GetBufferPointer(), sky.vertex_shader_buffer->GetBufferSize(), NULL, &sky.vertex_shader);
    device->CreatePixelShader(sky.pixel_shader_buffer->GetBufferPointer(), sky.pixel_shader_buffer->GetBufferSize(), NULL, &sky.pixel_shader);

	os_create_dds_texture_from_file(
		device,
		L"content\\skymap.dds",
		&sky.sky_tv.texture,
		&sky.sky_tv.shader_resource_view);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
    ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
    dssDesc.DepthEnable = true;
    dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device->CreateDepthStencilState(&dssDesc, &sky.depth_stencil);
}

void world::release_common_resources()
{
	if(swap_chain != nullptr) swap_chain->Release();
	swap_chain = nullptr;
	if(device != nullptr) device->Release();
	device = nullptr;
	if(device_context != nullptr) device_context->Release();
	device_context = nullptr;
	if(render_target_view != nullptr) render_target_view->Release();
	render_target_view = nullptr;
	if(depth_stencil_view != nullptr) depth_stencil_view->Release();
	depth_stencil_view = nullptr;
	if(depth_stencil_buffer != nullptr) depth_stencil_buffer->Release();
	depth_stencil_buffer = nullptr;
	if(cb_handler != nullptr) cb_handler->Release();
	cb_handler = nullptr;
	if(cb_joints_handler != nullptr) cb_joints_handler->Release();
	cb_joints_handler = nullptr;

	if(vertex_shader_buffer != nullptr) vertex_shader_buffer->Release();
	vertex_shader_buffer = nullptr;
	if(vertex_shader != nullptr) vertex_shader->Release();
	vertex_shader = nullptr;
	if(pixel_shader_buffer != nullptr) pixel_shader_buffer->Release();
	pixel_shader_buffer = nullptr;
	if(pixel_shader != nullptr) pixel_shader->Release();
	pixel_shader = nullptr;
	if(vertex_layout != nullptr) vertex_layout->Release();
	vertex_layout = nullptr;
	if(sampler_state != nullptr) sampler_state->Release();
	sampler_state = nullptr;

	if(render_target != nullptr) render_target->Release();
	render_target = nullptr;
	if(rt_view != nullptr) rt_view->Release();
	rt_view = nullptr;
	if(rt_shader_resource_view != nullptr) rt_shader_resource_view->Release();
	rt_shader_resource_view = nullptr;
}

void world::release_ui_resources()
{
	if(ui_vertex_shader_buffer != nullptr) ui_vertex_shader_buffer->Release();
	ui_vertex_shader_buffer = nullptr;
	if(ui_vertex_shader != nullptr) ui_vertex_shader->Release();
	ui_vertex_shader = nullptr;
	if(ui_pixel_shader_buffer != nullptr) ui_pixel_shader_buffer->Release();
	ui_pixel_shader_buffer = nullptr;
	if(ui_pixel_shader != nullptr) ui_pixel_shader->Release();
	ui_pixel_shader = nullptr;
	if(ui_texture != nullptr) ui_texture->Release();
	ui_texture = nullptr;
	if(ui_srv != nullptr) ui_srv->Release();
	ui_srv = nullptr;
}

void world::release_outline_resources()
{
	if(selection.hit_test_vertex_shader_buffer != nullptr) selection.hit_test_vertex_shader_buffer->Release();
	selection.hit_test_vertex_shader_buffer = nullptr;
	if(selection.hit_test_vertex_shader != nullptr) selection.hit_test_vertex_shader->Release();
	selection.hit_test_vertex_shader = nullptr;
	if(selection.hit_test_pixel_shader_buffer != nullptr) selection.hit_test_pixel_shader_buffer->Release();
	selection.hit_test_pixel_shader_buffer = nullptr;
	if(selection.hit_test_pixel_shader != nullptr) selection.hit_test_pixel_shader->Release();
	selection.hit_test_pixel_shader = nullptr;
	if(selection.outline_vertex_shader_buffer != nullptr) selection.outline_vertex_shader_buffer->Release();
	selection.outline_vertex_shader_buffer = nullptr;
	if(selection.outline_vertex_shader != nullptr) selection.outline_vertex_shader->Release();
	selection.outline_vertex_shader = nullptr;
	if(selection.outline_pixel_shader_buffer != nullptr) selection.outline_pixel_shader_buffer->Release();
	selection.outline_pixel_shader_buffer = nullptr;
	if(selection.outline_pixel_shader != nullptr) selection.outline_pixel_shader->Release();
	selection.outline_pixel_shader = nullptr;
}

void world::release_sky_resources()
{
    if(sky.vertex_shader_buffer != nullptr) sky.vertex_shader_buffer->Release();
	sky.vertex_shader_buffer = nullptr;
	if(sky.pixel_shader_buffer != nullptr) sky.pixel_shader_buffer->Release();
	sky.pixel_shader_buffer = nullptr;
	if(sky.vertex_shader != nullptr) sky.vertex_shader->Release();
	sky.vertex_shader = nullptr;
    if(sky.pixel_shader != nullptr) sky.pixel_shader->Release();
	sky.pixel_shader = nullptr;
    if(sky.depth_stencil != nullptr) sky.depth_stencil->Release();
	sky.depth_stencil = nullptr;
}

void world::initialize_scene()
{
	camera.position = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
	camera.target = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	camera.up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    camera.view = XMMatrixLookAtLH(camera.position, camera.target, camera.up);
    camera.projection = XMMatrixPerspectiveFovLH(
		0.4f * pi,
		float32(width) / float32(height),
		1.0f,
		1000.0f);

	movement.default_forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	movement.default_right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	movement.camera_forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	movement.camera_right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	movement.camera_yaw = 0.0f;
	movement.camera_pitch = 0.0f;
	movement.move_forward = false;
	movement.move_back = false;
	movement.move_right = false;
	movement.move_left = false;

	world_vertex v[] =
    {
        world_vertex(vector<float32, 3>(-1.0f, -1.0f, 0.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(1.0f, -1.0f, 0.0f),  vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(1.0f, 1.0f, 0.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f, 1.0f, 0.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f))
    };
	uiwo.vertices.insert_range(0, v, v + array_size(v));
    uint32 indices[] =
	{
        0, 2, 1,
        0, 3, 2,
    };
	uiwo.indices.insert_range(0, indices, indices + array_size(indices));
	uiwo.initialize_render_resources(device);

	world_vertex cube_vertices[] =
    {
        world_vertex(vector<float32, 3>(-1.0f, -1.0f, -1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f,  1.0f, -1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f,  1.0f, -1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, -1.0f, -1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    
        world_vertex(vector<float32, 3>(-1.0f, -1.0f, 1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, -1.0f, 1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f,  1.0f, 1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f,  1.0f, 1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    
        world_vertex(vector<float32, 3>(-1.0f, 1.0f, -1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f, 1.0f,  1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, 1.0f,  1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, 1.0f, -1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    
        world_vertex(vector<float32, 3>(-1.0f, -1.0f, -1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, -1.0f, -1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, -1.0f,  1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f, -1.0f,  1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    
        world_vertex(vector<float32, 3>(-1.0f, -1.0f,  1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f,  1.0f,  1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f,  1.0f, -1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>(-1.0f, -1.0f, -1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    
        world_vertex(vector<float32, 3>( 1.0f, -1.0f, -1.0f), vector<float32, 2>(0.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f,  1.0f, -1.0f), vector<float32, 2>(0.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f,  1.0f,  1.0f), vector<float32, 2>(1.0f, 0.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
        world_vertex(vector<float32, 3>( 1.0f, -1.0f,  1.0f), vector<float32, 2>(1.0f, 1.0f), vector<float32, 3>(0.0f, 0.0f, 0.0f)),
    };
    cube.vertices.insert_range(0, cube_vertices, cube_vertices + array_size(cube_vertices));
    uint32 cube_indices[] =
	{
        // Front Face
        0,  1,  2,
        0,  2,  3,
    
        // Back Face
        4,  5,  6,
        4,  6,  7,
    
        // Top Face
        8,  9, 10,
        8, 10, 11,
    
        // Bottom Face
        12, 13, 14,
        12, 14, 15,
    
        // Left Face
        16, 17, 18,
        16, 18, 19,
    
        // Right Face
        20, 21, 22,
        20, 22, 23
    };
	cube.indices.insert_range(0, cube_indices, cube_indices + array_size(cube_indices));
	cube.initialize_render_resources(device);

	os_create_texture_from_file(
		device,
		L"content\\braynzar.jpg",
		&cube_tv.texture,
		&cube_tv.shader_resource_view);

	generate_cylinder(10, &cylinder.vertices);
	cylinder.initialize_render_resources(device);

	models.push_default();
	gltf model;
	model.filename << u"A:\\Документы\\Kromash Engine\\content\\footman\\Footman_RIG.glb";
	model.device = device;
	model.model = &models[0];
	model.load();
	models[0].reset_animation(0);

	hm.resize(1000, 1000);
	hm.start_x = -500;
	hm.start_z = -500;
	hm.unit_size = 1.0f;
	for(uint64 i = 0; i < hm.length; i++)
		for(uint64 j = 0; j < hm.width; j++)
		{
			hm.map[i * hm.width + j].hole = false;
			hm.map[i * hm.width + j].height = 0.0f;
		}
	hm.generate_model(device);
}

void world::mouse_click()
{
	
}

void world::update_movement()
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(movement.camera_pitch, movement.camera_yaw, 0);
    movement.camera_right = XMVector3TransformCoord(movement.default_right, rotation);
    movement.camera_forward = XMVector3TransformCoord(movement.default_forward, rotation);

	float32 move_forward = 0.0f, move_right = 0.0f, dt = 0.000000001f * float32(now() - movement.last_move_time);
	if(movement.move_forward) move_forward += 2.0f * dt;
	if(movement.move_back) move_forward -= 2.0f * dt;
	if(movement.move_right) move_right += 2.0f * dt;
	if(movement.move_left) move_right -= 2.0f * dt;
	movement.last_move_time = now();
    camera.position += move_right * movement.camera_right;
    camera.position += move_forward * movement.camera_forward;
	
    camera.target = camera.position + movement.camera_forward;
    camera.up = XMVector3Cross(movement.camera_forward, movement.camera_right);

    camera.view = XMMatrixLookAtLH(camera.position, camera.target, camera.up);
}

void world::update_fps()
{
	timestamp t = now() / 1000000000;
	if(t != fps_second_accumulative)
	{
		fps_second_accumulative = t;
		fps = fps_accumulative;
		fps_accumulative = 1;
	}
	else fps_accumulative++;
}

void world::resize()
{
	device_context->OMSetRenderTargets(0, NULL, NULL);
    render_target_view->Release();
	depth_stencil_buffer->Release();
	depth_stencil_view->Release();
	swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D *buffer;
    swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)(&buffer));
    device->CreateRenderTargetView(buffer, NULL, &render_target_view);
    buffer->Release();
	D3D11_TEXTURE2D_DESC depth_stencil_desc;
    depth_stencil_desc.Width = width;
    depth_stencil_desc.Height = height;
    depth_stencil_desc.MipLevels = 1;
    depth_stencil_desc.ArraySize = 1;
    depth_stencil_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_stencil_desc.SampleDesc.Count = 1;
    depth_stencil_desc.SampleDesc.Quality = 0;
    depth_stencil_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_stencil_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_stencil_desc.CPUAccessFlags = 0; 
    depth_stencil_desc.MiscFlags = 0;
    device->CreateTexture2D(&depth_stencil_desc, NULL, &depth_stencil_buffer);
    device->CreateDepthStencilView(depth_stencil_buffer, NULL, &depth_stencil_view);

	render_target->Release();
	rt_view->Release();
	rt_shader_resource_view->Release();

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
	D3D11_TEXTURE2D_DESC texture_desc;
    ZeroMemory(&texture_desc, sizeof(texture_desc));
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = 0;
    device->CreateTexture2D(&texture_desc, NULL, &render_target);

    D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
    render_target_view_desc.Format = texture_desc.Format;
    render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    render_target_view_desc.Texture2D.MipSlice = 0;
    device->CreateRenderTargetView(render_target, &render_target_view_desc, &rt_view);

	shader_resource_view_desc.Format = texture_desc.Format;
    shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
    shader_resource_view_desc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(render_target, &shader_resource_view_desc, &rt_shader_resource_view);

	release_ui_resources();
	initialize_ui_resources();

	camera.projection = XMMatrixPerspectiveFovLH(
		0.4f * pi,
		float32(width) / float32(height),
		1.0f,
		1000.0f);
}

void world::render_heightmap()
{
	ID3D11RasterizerState *rs;
	D3D11_RASTERIZER_DESC rs_desc;
	rs_desc.AntialiasedLineEnable = false;
	rs_desc.CullMode = D3D11_CULL_BACK;
	rs_desc.DepthBias = 0;
	rs_desc.DepthBiasClamp = 0.0f;
	rs_desc.DepthClipEnable = true;
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.FrontCounterClockwise = false;
	rs_desc.MultisampleEnable = false;
	rs_desc.ScissorEnable = false;
	rs_desc.SlopeScaledDepthBias = 0.0f;
	device->CreateRasterizerState(&rs_desc, &rs);
	device_context->RSSetState(rs);
	rs->Release();

	device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uint32 offset = 0;
	uint32 stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &hm.map_model.vertex_buffer, &stride, &offset);
    device_context->IASetIndexBuffer(hm.map_model.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	cb.world = XMMatrixIdentity();
    cb.wvp = cb.world * camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
	cb.animated = 0;
	cb.pbr = 0;
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetShaderResources(0, 1, &cube_tv.shader_resource_view);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
	cb.manual_uv = 0;
	device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
	device_context->DrawIndexed(hm.map_model.indices.size, 0, 0);
	/*for(uint64 i = 0; i < hm.length - 1; i++)
	{
		for(uint64 j = 0; j < hm.width - 1; j++)
			device_context->DrawIndexed(6, (i * (width - 1) + j) * 6, 0);
	}*/
}

void world::render_sky()
{
	ID3D11RasterizerState *rs;
	D3D11_RASTERIZER_DESC rs_desc;
	rs_desc.AntialiasedLineEnable = false;
	rs_desc.CullMode = D3D11_CULL_NONE;
	rs_desc.DepthBias = 0;
	rs_desc.DepthBiasClamp = 0.0f;
	rs_desc.DepthClipEnable = true;
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.FrontCounterClockwise = false;
	rs_desc.MultisampleEnable = false;
	rs_desc.ScissorEnable = false;
	rs_desc.SlopeScaledDepthBias = 0.0f;
	device->CreateRasterizerState(&rs_desc, &rs);
	device_context->RSSetState(rs);
	rs->Release();
	
	device_context->VSSetShader(sky.vertex_shader, 0, 0);
    device_context->PSSetShader(sky.pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uint32 offset = 0;
	uint32 stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &sky.sphere.vertex_buffer, &stride, &offset);
	device_context->IASetIndexBuffer(sky.sphere.index_buffer, DXGI_FORMAT_R32_UINT, 0);
    sky.sphereWorld = XMMatrixScaling(5.0f, 5.0f, 5.0f)
		* XMMatrixTranslation(XMVectorGetX(camera.position), XMVectorGetY(camera.position), XMVectorGetZ(camera.position));
    cb.wvp = sky.sphereWorld * camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
    cb.world = XMMatrixTranspose(sky.sphereWorld);
	cb.animated = 0;
	cb.manual_uv = 0;
    device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
    device_context->PSSetShaderResources(4, 1, &sky.sky_tv.shader_resource_view);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(sky.depth_stencil, 0);
    device_context->DrawIndexed(sky.sphere.indices.size, 0, 0);
}

void world::render_objects()
{
	ID3D11RasterizerState *rs;
	D3D11_RASTERIZER_DESC rs_desc;
	rs_desc.AntialiasedLineEnable = false;
	rs_desc.CullMode = D3D11_CULL_BACK;
	rs_desc.DepthBias = 0;
	rs_desc.DepthBiasClamp = 0.0f;
	rs_desc.DepthClipEnable = true;
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.FrontCounterClockwise = false;
	rs_desc.MultisampleEnable = false;
	rs_desc.ScissorEnable = false;
	rs_desc.SlopeScaledDepthBias = 0.0f;
	device->CreateRasterizerState(&rs_desc, &rs);
	device_context->RSSetState(rs);
	rs->Release();

	device_context->VSSetShader(vertex_shader, 0, 0);
    device_context->PSSetShader(pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uint32 offset = 0;
	uint32 stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &cube.vertex_buffer, &stride, &offset);
    device_context->IASetIndexBuffer(cube.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	cb.world = XMMatrixTranslation(0.0f, 0.0f, 15.0f);
    cb.wvp = cb.world * camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
	cb.animated = 0;
	cb.manual_uv = 0;
	cb.pbr = 0;
    device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetShaderResources(0, 1, &cube_tv.shader_resource_view);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
    device_context->DrawIndexed(cube.indices.size, 0, 0);

	models[0].play_animation(0);
	cb.animated = 1;
	for(uint64 i = 0; i < models.size; i++)
	{
		for(uint64 j = 0; j < models[i].joints.size; j++)
			cb_joints.final_transform[j] = XMMatrixTranspose(models[i].joints[j].final_transform);
		device_context->UpdateSubresource(cb_joints_handler, 0, NULL, &cb_joints, 0, 0);
		device_context->VSSetConstantBuffers(1, 1, &cb_joints_handler);
		for(uint64 j = 0; j < models[i].subsets.size; j++)
		{
			device_context->IASetVertexBuffers(0, 1, &models[i].subsets[j].vertex_buffer, &stride, &offset);
			device_context->IASetIndexBuffer(models[i].subsets[j].index_buffer, DXGI_FORMAT_R32_UINT, 0);
			cb.world = XMMatrixIdentity();
			cb.wvp = camera.view * camera.projection;
			cb.wvp = XMMatrixTranspose(cb.wvp);
			if(models[i].subsets[j].material.base_color_texture != not_exists<uint64>
				&& models[i].subsets[j].material.metallic_texture != not_exists<uint64>
				&& models[i].subsets[j].material.normal_texture != not_exists<uint64>)
			{
				cb.pbr = 1;
				cb.metallic_factor = models[i].subsets[j].material.metallic_factor * 0.5f;
				cb.roughness_factor = models[i].subsets[j].material.roughness_factor * 0.5f;
				cb.baseColorFactor = models[i].subsets[j].material.base_color_factor;
				device_context->PSSetShaderResources(0, 1, &models[i].textures[models[i].subsets[j].material.base_color_texture].shader_resource_view);
				device_context->PSSetShaderResources(1, 1, &models[i].textures[models[i].subsets[j].material.metallic_texture].shader_resource_view);
				device_context->PSSetShaderResources(2, 1, &models[i].textures[models[i].subsets[j].material.normal_texture].shader_resource_view);
			}
			else
			{
				cb.pbr = 0;
				device_context->PSSetShaderResources(0, 1, &models[i].textures[models[i].subsets[j].material.base_color_texture].shader_resource_view);
			}
			device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
			device_context->DrawIndexed(models[i].subsets[j].indices.size, 0, 0);
		}
	}
}

void world::render_outline()
{
	ID3D11RasterizerState *rs;
	D3D11_RASTERIZER_DESC rs_desc;
	rs_desc.AntialiasedLineEnable = false;
	rs_desc.CullMode = D3D11_CULL_BACK;
	rs_desc.DepthBias = 0;
	rs_desc.DepthBiasClamp = 0.0f;
	rs_desc.DepthClipEnable = true;
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.FrontCounterClockwise = false;
	rs_desc.MultisampleEnable = false;
	rs_desc.ScissorEnable = false;
	rs_desc.SlopeScaledDepthBias = 0.0f;
	device->CreateRasterizerState(&rs_desc, &rs);
	device_context->RSSetState(rs);
	rs->Release();

	device_context->OMSetRenderTargets(1, &rt_view, nullptr);
	D3D11_VIDEO_COLOR_RGBA bg_color {0.0f, 0.0f, 0.0f, 0.0f};
    device_context->ClearRenderTargetView(rt_view, (float32 *)(&bg_color));
    device_context->VSSetShader(selection.hit_test_vertex_shader, 0, 0);
    device_context->PSSetShader(selection.hit_test_pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    uint32 offset = 0;
	uint32 stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &cube.vertex_buffer, &stride, &offset);
    device_context->IASetIndexBuffer(cube.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	cb.world = XMMatrixTranslation(0.0f, 0.0f, 15.0f);
    cb.wvp = cb.world * camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
	cb.animated = 0;
	cb.manual_uv = 0;
    device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
    device_context->DrawIndexed(cube.indices.size, 0, 0);

	device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
	device_context->VSSetShader(selection.outline_vertex_shader, 0, 0);
    device_context->PSSetShader(selection.outline_pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    offset = 0;
	stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &uiwo.vertex_buffer, &stride, &offset);
    device_context->IASetIndexBuffer(uiwo.index_buffer, DXGI_FORMAT_R32_UINT, 0);
	cb.world = XMMatrixTranslation(0.0f, 0.0f, 15.0f);
    cb.wvp = cb.world * camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
	cb.color = vector<float32, 4>(1.0f, 0.0f, 1.0f, 1.0f);
	cb.radius = 10;
    device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetShaderResources(0, 1, &rt_shader_resource_view);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
    device_context->DrawIndexed(uiwo.indices.size, 0, 0);

	/*device_context->VSSetShader(selection.outline_vertex_shader, 0, 0);
    device_context->PSSetShader(selection.outline_pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    uint32 offset = 0;
	uint32 stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &selection.outline.vertex_buffer, &stride, &offset);
    cb.WVP = camera.view * camera.projection;
    cb.WVP = XMMatrixTranspose(cb.WVP);
	cb.color = vector<float32, 4>(1.0f, 0.0f, 1.0f, 1.0f);
    device_context->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
    device_context->Draw(selection.outline.vertices.size, 0);*/
	
	/*device_context->VSSetShader(selection.outline_vertex_shader, 0, 0);
    device_context->PSSetShader(selection.outline_pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
	cb.color = vector<float32, 4>(1.0f, 0.0f, 1.0f, 1.0f);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	offset = 0;
	stride = sizeof(world_vertex);
	device_context->IASetVertexBuffers(0, 1, &cylinder.vertex_buffer, &stride, &offset);
	device_context->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);

	vector<float32, 3> v, a, p, q,
		vx = vector<float32, 3>(1.0f, 0.0f, 0.0f),
		vy = vector<float32, 3>(0.0f, 1.0f, 0.0f),
		vz = vector<float32, 3>(0.0f, 0.0f, 1.0f);
	matrix<float32, 3, 3> m;
	float32 pitch, yaw, roll, l1, l2;
	for(uint64 i = 0; i < selection.outline.vertices.size; i += 2)
	{
		q = selection.outline.vertices[i].point;
		p = selection.outline.vertices[i + 1].point;
		v = p - q;
		a = vector_normal(v);
		pitch = acos(a.y);
		if(a.z < 0.0f) pitch = -pitch;
		XMVECTOR xmv = XMVector3TransformCoord(
			XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
			XMMatrixRotationRollPitchYaw(pitch, 0.0f, 0.0f));
		v.x = xmv.m128_f32[0];
		v.y = xmv.m128_f32[1];
		v.z = xmv.m128_f32[2];
		l1 = vector_length(vector<float32, 3>(a.x, 0.0f, a.z));
		l2 = vector_length(vector<float32, 3>(v.x, 0.0f, v.z));
		if(l1 < 0.0001f || l2 < 0.0001f) yaw = 0.0f;
		else
		{
			yaw = acos((a.x *  v.x + a.z * v.z) / (l1 * l2));
			if(a.x < 0.0f) yaw = -yaw;
		}
		roll = 0.0f;
		cb.WVP = XMMatrixScaling(0.1f, vector_length(p - q), 0.1f)
			* XMMatrixRotationRollPitchYaw(pitch, yaw, roll)
			* XMMatrixTranslation(
				q.x,
				q.y,
				q.z)
			* camera.view * camera.projection;
		cb.WVP = XMMatrixTranspose(cb.WVP);
		device_context->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cb, 0, 0);
		device_context->Draw(cylinder.vertices.size, 0);
	}*/
}

void world::render_ui(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	world_frame *wf = (world_frame *)(fm.object.addr);
	layout.core->x = wf->fm.x;
	layout.core->y = wf->fm.y;
	layout.core->width = wf->fm.width;
	layout.core->height = wf->fm.height;
	layout.core->render(layout.object, gd, bmp);

	D3D11_MAPPED_SUBRESOURCE mapped;
	device_context->Map(ui_texture, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	alpha_color *src = bmp->data, *dest = (alpha_color *)(mapped.pData), *dest_prev;
	vector<int32, 2> position = os_window_content_position(wnd);
	vector<uint32, 2> size = os_window_content_size(wnd);
	int32 xs, ys = position.y, xsize;
	if(ys < 0)
	{
		shift_addr(&dest, int64(mapped.RowPitch * uint32(-ys)));
		ys = 0;
	}
	dest_prev = dest;
	while(ys < min(position.y + int32(size.y), int32(bmp->height)))
	{
		xs = position.x;
		xsize = size.x;
		if(xs < 0)
		{
			shift_addr(&dest, int64((-xs) * sizeof(alpha_color)));
			xsize += xs;
			xs = 0;
		}
		xsize = min(xsize, int32(bmp->width) - xs);
		if(xsize > 0)
			copy_memory(bmp->data + (bmp->height - 1 - ys) * int32(bmp->width) + xs, dest, uint64(xsize * sizeof(alpha_color)));
		dest = dest_prev;
		shift_addr(&dest, mapped.RowPitch);
		dest_prev = dest;
		ys++;
	}
	device_context->Unmap(ui_texture, 0);

	ID3D11RasterizerState *rs;
	D3D11_RASTERIZER_DESC rs_desc;
	rs_desc.AntialiasedLineEnable = false;
	rs_desc.CullMode = D3D11_CULL_BACK;
	rs_desc.DepthBias = 0;
	rs_desc.DepthBiasClamp = 0.0f;
	rs_desc.DepthClipEnable = true;
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.FrontCounterClockwise = false;
	rs_desc.MultisampleEnable = false;
	rs_desc.ScissorEnable = false;
	rs_desc.SlopeScaledDepthBias = 0.0f;
	device->CreateRasterizerState(&rs_desc, &rs);
	device_context->RSSetState(rs);
	rs->Release();

    device_context->VSSetShader(ui_vertex_shader, 0, 0);
    device_context->PSSetShader(ui_pixel_shader, 0, 0);
    device_context->IASetInputLayout(vertex_layout);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT offset = 0;
	UINT stride = sizeof(world_vertex);
    device_context->IASetVertexBuffers(0, 1, &uiwo.vertex_buffer, &stride, &offset );
    device_context->IASetIndexBuffer(uiwo.index_buffer, DXGI_FORMAT_R32_UINT, 0);
    cb.wvp = camera.view * camera.projection;
    cb.wvp = XMMatrixTranspose(cb.wvp);
	cb.animated = 0;
	cb.manual_uv = 0;
    device_context->UpdateSubresource(cb_handler, 0, NULL, &cb, 0, 0);
    device_context->VSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetConstantBuffers(0, 1, &cb_handler);
	device_context->PSSetShaderResources(0, 1, &ui_srv);
    device_context->PSSetSamplers(0, 1, &sampler_state);
    device_context->OMSetDepthStencilState(nullptr, 0);
    device_context->DrawIndexed(uiwo.indices.size, 0, 0);
}

void world::render(handleable<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	world_frame *wf = (world_frame *)(fm.object.addr);
	if(width != wf->fm.width || height != wf->fm.height)
	{
		width = wf->fm.width;
		height = wf->fm.height;
		resize();
	}
	
	device_context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);
	D3D11_VIDEO_COLOR_RGBA bg_color {1.0f, 1.0f, 0.0f, 1.0f};
    device_context->ClearRenderTargetView(render_target_view, (float32 *)(&bg_color));
	device_context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = float32(width);
    viewport.Height = float32(height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
    device_context->RSSetViewports(1, &viewport);
	cb.width = width;
	cb.height = height;

	cb.light_dir = vector<float32, 3>(1.0f, 0.0f, 0.0f);
	cb.light_color = vector<float32, 3>(1.0f, 1.0f, 1.0f);
	cb.camera = vector<float32, 3>(camera.position.m128_f32[0], camera.position.m128_f32[1], camera.position.m128_f32[2]);

	update_fps();
	update_movement();
	render_heightmap();
	render_sky();
	render_objects();
	render_outline();
	render_ui(fm, gd, bmp);
    swap_chain->Present(0, 0);
}

bool world_frame_hit_test(indefinite<frame> fm, vector<int32, 2> point)
{
	world_frame *wf = (world_frame *)(fm.addr);
	return rectangle<int32>(
		vector<int32, 2>(wf->fm.x, wf->fm.y),
		vector<int32, 2>(wf->fm.width, wf->fm.height)).hit_test(point);
}

void world_frame_subframes(indefinite<frame> fm, array<handleable<frame>> *frames)
{
	world_frame *wf = (world_frame *)(fm.addr);
	frames->push(wf->wld.layout);
}

void world_frame_mouse_click(indefinite<frame> fm)
{
	world_frame *wf = (world_frame *)(fm.addr);
	wf->wld.mouse_click();
}

void world_frame_mouse_move(indefinite<frame> fm)
{
	const float32 limit = pi / 2.0f - 0.001f;
	world_frame *wf = (world_frame *)(fm.addr);
	wf->wld.movement.camera_pitch += 0.01f * float32(mouse()->prev_position.y - mouse()->position.y);
	if(wf->wld.movement.camera_pitch > limit) wf->wld.movement.camera_pitch = limit;
	else if(wf->wld.movement.camera_pitch < -limit) wf->wld.movement.camera_pitch = -limit;
	wf->wld.movement.camera_yaw += 0.01f * float32(mouse()->position.x - mouse()->prev_position.x);
}

void world_frame_key_press(indefinite<frame> fm)
{
	world_frame *wf = (world_frame *)(fm.addr);
	if(keyboard()->key_pressed[uint32(key_code::w)])
		wf->wld.movement.move_forward = true;
	if(keyboard()->key_pressed[uint32(key_code::s)])
		wf->wld.movement.move_back = true;
	if(keyboard()->key_pressed[uint32(key_code::d)])
		wf->wld.movement.move_right = true;
	if(keyboard()->key_pressed[uint32(key_code::a)])
		wf->wld.movement.move_left = true;
	wf->wld.movement.last_move_time = now();
}

void world_frame_key_release(indefinite<frame> fm)
{
	world_frame *wf = (world_frame *)(fm.addr);
	if(!keyboard()->key_pressed[uint32(key_code::w)])
		wf->wld.movement.move_forward = false;
	if(!keyboard()->key_pressed[uint32(key_code::s)])
		wf->wld.movement.move_back = false;
	if(!keyboard()->key_pressed[uint32(key_code::d)])
		wf->wld.movement.move_right = false;
	if(!keyboard()->key_pressed[uint32(key_code::a)])
		wf->wld.movement.move_left = false;
}

void world_frame_render(indefinite<frame> fm, graphics_displayer *gd, bitmap *bmp)
{
	world_frame *wf = (world_frame *)(fm.addr);
	wf->wld.render(handleable<frame>(wf, &wf->fm), gd, bmp);
}

world_frame::world_frame()
{
	fm.hit_test = world_frame_hit_test;
	fm.subframes = world_frame_subframes;
	fm.render = world_frame_render;
	fm.mouse_click.callbacks.push(world_frame_mouse_click);
	fm.mouse_move.callbacks.push(world_frame_mouse_move);
	fm.key_press.callbacks.push(world_frame_key_press);
	fm.key_release.callbacks.push(world_frame_key_release);
}
