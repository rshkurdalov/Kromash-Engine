#include "world_object.h"
#include "time.h"

texture_view::texture_view()
{
	texture = nullptr;
	shader_resource_view = nullptr;
}

texture_view::~texture_view()
{
	if(texture != nullptr) texture->Release();
	if(shader_resource_view != nullptr) shader_resource_view->Release();
}

world_object::world_object()
{
	vertex_buffer = nullptr;
	index_buffer = nullptr;
}

world_object::~world_object()
{
	release_render_resources();
}

void world_object::initialize_render_resources(ID3D11Device *device)
{
    D3D11_BUFFER_DESC vertexBufferDesc;
    ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(world_vertex) * vertices.size;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;
    vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData; 
    ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
    vertexBufferData.pSysMem = vertices.addr;
    device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &vertex_buffer);

	D3D11_BUFFER_DESC indexBufferDesc;
    ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(DWORD) * indices.size;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData;
    iinitData.pSysMem = indices.addr;
    device->CreateBuffer(&indexBufferDesc, &iinitData, &index_buffer);
}

void world_object::release_render_resources()
{
	if(vertex_buffer != nullptr) vertex_buffer->Release();
	vertex_buffer = nullptr;
	if(index_buffer != nullptr) index_buffer->Release();
	index_buffer = nullptr;
}

void generate_cylinder(uint32 long_lines, array<world_vertex> *vertices)
{
    array<vector<float32, 3>> points;
    float32 a = 0.0f, da = 2.0f * pi / float32(long_lines);
    for(uint32 i = 0; i < long_lines; i++, a += da)
        points.push(vector<float32, 3>(cos(a), 0.0f, sin(a)));
    for(uint32 i = 0, prev_i = long_lines - 1; i < long_lines; i++, prev_i = i - 1)
    {
        vertices->push(world_vertex(points[prev_i]));
        vertices->push(world_vertex(points[i]));
        vertices->push(world_vertex(vector<float32, 3>(0.0f, 0.0f, 0.0f)));
        vertices->push(world_vertex(vector<float32, 3>(points[i].x, 1.0f, points[i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[prev_i].x, 1.0f, points[prev_i].z)));
        vertices->push(world_vertex(vector<float32, 3>(0.0f, 1.0f, 0.0f)));
        vertices->push(world_vertex(vector<float32, 3>(points[i].x, 0.0f, points[i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[prev_i].x, 0.0f, points[prev_i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[prev_i].x, 1.0f, points[prev_i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[prev_i].x, 1.0f, points[prev_i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[i].x, 1.0f, points[i].z)));
        vertices->push(world_vertex(vector<float32, 3>(points[i].x, 0.0f, points[i].z)));

    }
}

void world_model::reset_animation(uint32 animation)
{
	animations[animation].start_animation_time = now();
}

void world_model::play_animation(uint32 animation)
{
	float32 t = float32(float64(now() - animations[animation].start_animation_time) / 1000000000.0);
	if(t >= animations[animation].total_animation_time)
	{
		animations[animation].start_animation_time = now();
		t = 0.0f;
	}
	float32 interpolation;
	for(uint64 i = 0; i < nodes.size; i++)
	{
		nodes[i].scale = XMMatrixIdentity();
		nodes[i].rotation = XMMatrixIdentity();
		nodes[i].translation = XMMatrixIdentity();
	}
	for(uint64 i = 0; i < animations[animation].channels.size; i++)
	{
		for(uint64 j = 0; j < animations[animation].channels[i].timestamps.size - 1; j++)
		{
			if(t >= animations[animation].channels[i].timestamps[j + 1]) continue;
			interpolation = (t - animations[animation].channels[i].timestamps[j])
				/ (animations[animation].channels[i].timestamps[j + 1] - animations[animation].channels[i].timestamps[j]);
			if(animations[animation].channels[i].target == model_channel_target::scaling)
			{
				nodes[animations[animation].channels[i].node].scale = XMMatrixScaling(
					animations[animation].channels[i].frames[j].x * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].x * interpolation,
					animations[animation].channels[i].frames[j].y * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].y * interpolation,
					animations[animation].channels[i].frames[j].z * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].z * interpolation);
			}
			else if(animations[animation].channels[i].target == model_channel_target::rotating)
			{
				XMVECTOR joint0_orientation = XMVectorSet(
					animations[animation].channels[i].frames[j].x,
					animations[animation].channels[i].frames[j].y,
					animations[animation].channels[i].frames[j].z,
					animations[animation].channels[i].frames[j].w);
				XMVECTOR joint1_orientation = XMVectorSet(
					animations[animation].channels[i].frames[j + 1].x,
					animations[animation].channels[i].frames[j + 1].y,
					animations[animation].channels[i].frames[j + 1].z,
					animations[animation].channels[i].frames[j + 1].w);
				XMVECTOR v = XMQuaternionSlerp(joint0_orientation, joint1_orientation, interpolation);
				nodes[animations[animation].channels[i].node].rotation = XMMatrixRotationQuaternion(v);
			}
			else if(animations[animation].channels[i].target == model_channel_target::translating)
			{
				nodes[animations[animation].channels[i].node].translation = XMMatrixTranslation(
					animations[animation].channels[i].frames[j].x * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].x * interpolation,
					animations[animation].channels[i].frames[j].y * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].y * interpolation,
					animations[animation].channels[i].frames[j].z * (1.0 - interpolation)
					+ animations[animation].channels[i].frames[j + 1].z * interpolation);
			}
			break;
		}
	}
	int32 node;
	for(uint64 i = 0; i < nodes.size; i++)
	{
		nodes[i].global_transform = nodes[i].scale * nodes[i].rotation * nodes[i].translation;
		node = int32(i);
		while(nodes[node].parent != not_exists<int32>)
		{
			node = nodes[node].parent;
			nodes[i].global_transform = nodes[i].global_transform * nodes[node].scale * nodes[node].rotation * nodes[node].translation;
		}
	}
	for(uint64 i = 0; i < joints.size; i++)
		joints[i].final_transform = joints[i].inverse_bind_matrix * nodes[joints[i].node].global_transform * root_inverse_matrix;
}
