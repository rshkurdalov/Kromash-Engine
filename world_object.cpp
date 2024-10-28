#include "world_object.h"

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
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC; //!!!
    vertexBufferDesc.ByteWidth = sizeof(world_vertex) * vertices.size;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; //!!!
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

void world_model::run_animation(ID3D11DeviceContext *device_context, uint32 animation_idx, float32 t)
{
	float32 interpolation;
	array<model_joint> interpolated_skeleton;
	model_joint temp_joint;
	interpolated_skeleton.insert_default(0, joints.size);
	for(uint64 i = 0; i < interpolated_skeleton.size; i++)
	{
		interpolated_skeleton[i].scale = vector<float32, 3>(1.0f, 1.0f, 1.0f);
		interpolated_skeleton[i].orientation = vector<float32, 4>(1.0f, 0.0f, 0.0f, 0.0f);
		interpolated_skeleton[i].position = vector<float32, 3>(0.0f, 0.0f, 0.0f);
	}
	for(uint64 i = 0; i < animations[animation_idx].frame_skeleton.size; i++)
	{
		for(uint64 j = 0; j < animations[animation_idx].frame_skeleton[i].timestamps.size - 1; j++)
		{
			if(t >= animations[animation_idx].frame_skeleton[i].timestamps[j + 1]) continue;
			interpolation = (t - animations[animation_idx].frame_skeleton[i].timestamps[j])
				/ (animations[animation_idx].frame_skeleton[i].timestamps[j + 1] - animations[animation_idx].frame_skeleton[i].timestamps[j]);
			if(animations[animation_idx].frame_skeleton[i].frame_type == joint_frame_transform_type::scaling)
			{
				interpolated_skeleton[animations[animation_idx].frame_skeleton[i].joint_idx].scale = vector<float32, 3>(
					animations[animation_idx].frame_skeleton[i].frames[j].x * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].x * interpolation,
					animations[animation_idx].frame_skeleton[i].frames[j].y * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].y * interpolation,
					animations[animation_idx].frame_skeleton[i].frames[j].z * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].z * interpolation);
			}
			else if(animations[animation_idx].frame_skeleton[i].frame_type == joint_frame_transform_type::rotating)
			{
				XMVECTOR joint0_orientation = XMVectorSet(
					animations[animation_idx].frame_skeleton[i].frames[j].x,
					animations[animation_idx].frame_skeleton[i].frames[j].y,
					animations[animation_idx].frame_skeleton[i].frames[j].z,
					animations[animation_idx].frame_skeleton[i].frames[j].w);
				XMVECTOR joint1_orientation = XMVectorSet(
					animations[animation_idx].frame_skeleton[i].frames[j + 1].x,
					animations[animation_idx].frame_skeleton[i].frames[j + 1].y,
					animations[animation_idx].frame_skeleton[i].frames[j + 1].z,
					animations[animation_idx].frame_skeleton[i].frames[j + 1].w);
				XMFLOAT4 v;
				XMStoreFloat4(&v, XMQuaternionSlerp(joint0_orientation, joint1_orientation, interpolation));
				interpolated_skeleton[animations[animation_idx].frame_skeleton[i].joint_idx].orientation = vector<float32, 4>(v.x, v.y, v.z, v.w);
			}
			else if(animations[animation_idx].frame_skeleton[i].frame_type == joint_frame_transform_type::translating)
			{
				interpolated_skeleton[animations[animation_idx].frame_skeleton[i].joint_idx].position = vector<float32, 3>(
					animations[animation_idx].frame_skeleton[i].frames[j].x * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].x * interpolation,
					animations[animation_idx].frame_skeleton[i].frames[j].y * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].y * interpolation,
					animations[animation_idx].frame_skeleton[i].frames[j].z * (1.0 - interpolation)
					+ animations[animation_idx].frame_skeleton[i].frames[j + 1].z * interpolation);
			}
			break;
		}
	}
	for(uint64 k = 0; k < subsets.size; k++)
	{
		for(uint64 i = 0; i < subsets[k].vertices.size; i++)
		{
			world_vertex temp_vertex = subsets[k].vertices[i];
			temp_vertex.point = vector<float32, 3>(0.0f, 0.0f, 0.0f);
			temp_vertex.normal = vector<float32, 3>(0.0f, 0.0f, 0.0f);
			for(uint32 j = 0; j < temp_vertex.weight_count; j++)
			{
				model_weight temp_weight = subsets[k].weights[temp_vertex.weight_idx + j];
				model_joint temp_joint = interpolated_skeleton[temp_weight.joint_idx];
				XMMATRIX rotation = XMMatrixRotationQuaternion(XMVectorSet(temp_joint.orientation.x, temp_joint.orientation.y, temp_joint.orientation.z, temp_joint.orientation.w)),
					translation = XMMatrixTranslation(temp_joint.position.x, temp_joint.position.y, temp_joint.position.z);
				XMVECTOR v = XMVector3TransformCoord(
					XMVectorSet(subsets[k].vertices[i].point.x, subsets[k].vertices[i].point.y, subsets[k].vertices[i].point.z, 1.0f),
					rotation * translation);
				temp_vertex.point.x += v.m128_f32[0] * temp_weight.bias;
				temp_vertex.point.y += v.m128_f32[1] * temp_weight.bias;
				temp_vertex.point.z += v.m128_f32[2] * temp_weight.bias;
			}
			subsets[k].positions[i] = temp_vertex.point;
		}
		array<world_vertex> transformed_vertices;
		for(uint64 i = 0; i < subsets[k].vertices.size; i++)
		{
			transformed_vertices.push(subsets[k].vertices[i]);
			transformed_vertices[i].point = subsets[k].positions[i];
		}
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = device_context->Map(subsets[k].vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, transformed_vertices.addr, sizeof(world_vertex) * transformed_vertices.size);
		device_context->Unmap(subsets[k].vertex_buffer, 0);
	}
}
