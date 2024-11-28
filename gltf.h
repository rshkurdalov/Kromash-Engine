#pragma once
#include "json.h"
#include "world_object.h"
#include "os_api.h"

struct gltf
{
	u16string filename;
	ID3D11Device *device;
	world_model *model;
	file f;
	json<char8> json_data;
	uint64 buffer_begin;
	uint64 idx;
	json_node<char8> *meshes;
	json_node<char8> *accessor;
	json_node<char8> *buffer;
	json_node<char8> *buffer_view;
	json_node<char8> *material;
	json_node<char8> *texture;
	json_node<char8> *image;
	json_node<char8> *animations;
	json_node<char8> *nodes;
	json_node<char8> *channel;
	json_node<char8> *sampler;
	json_node<char8> *skin;

	bool load()
	{
		static constexpr uint32 component_type_byte = 5120;
		static constexpr uint32 component_type_ubyte = 5121;
		static constexpr uint32 component_type_short = 5122;
		static constexpr uint32 component_type_ushort = 5123;
		static constexpr uint32 component_type_uint = 5125;
		static constexpr uint32 component_type_float = 5126;

		try
		{
			f.filename << filename;
			f.open();
			if(f.status != filestatus::opened) return false;
			u16string format;
			uint64 filename_idx = f.filename.size - 1;
			while(filename_idx != not_exists<uint64> && f.filename[filename_idx] != u'.')
				filename_idx--;
			if(filename_idx == not_exists<uint64>) return false;
			format.insert_range(0, f.filename.addr + filename_idx, f.filename.addr + f.filename.size);
			if(format == u16string(u".gltf"))
			{
	
			}
			else if(format == u16string(u".glb"))
			{
				uint32 json_size;
				f.position = 12;
				if(f.read(4, &json_size) != 4) return false;
				f.position = 20;
				u8string json_str;
				json_str.insert_default(0, json_size);
				if(f.read(json_size, json_str.addr) != json_size) return false;
				if(!json_data.load(json_str).success) return false;

				buffer_begin = 20 + uint64(json_size) + 8;
				meshes = &json_data.root["meshes"];
				buffer = &json_data.root["buffers"][0];
				array<vector<float32, 2>> vec2f_array;
				array<vector<float32, 3>> vec3f_array;
				array<vector<uint16, 4>> vec4u16_array;
				array<vector<float32, 4>> vec4f_array;
				array<uint16> u16_array;
				for(uint64 i = 0; i < meshes->values.size; i++)
				{
					model->subsets.push_default();

					accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["attributes"]["POSITION"].numeric_value)];
					if((*accessor)["type"].string_value == u8string("VEC3")
						&& uint32((*accessor)["componentType"].numeric_value) == component_type_float)
					{
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						model->subsets[i].vertices.insert_default(0, uint64((*accessor)["count"].numeric_value));
						vec3f_array.insert_default(0, model->subsets[i].vertices.size);
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						f.read(model->subsets[i].vertices.size * sizeof(vector<float32, 3>), vec3f_array.addr);
						for(uint64 j = 0; j < model->subsets[i].vertices.size; j++)
							model->subsets[i].vertices[j].point = vec3f_array[j];
						vec3f_array.clear();
					}

					accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["indices"].numeric_value)];
					buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
					model->subsets[i].indices.insert_default(0, uint64((*accessor)["count"].numeric_value));
					f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
					if(uint32((*accessor)["componentType"].numeric_value) == component_type_uint)
						f.read(model->subsets[i].indices.size * sizeof(uint32), model->subsets[i].indices.addr);
					else if(uint32((*accessor)["componentType"].numeric_value) == component_type_short
						|| uint32((*accessor)["componentType"].numeric_value) == component_type_ushort)
					{
						u16_array.insert_default(0, model->subsets[i].indices.size);
						f.read(model->subsets[i].indices.size * sizeof(uint16), u16_array.addr);
						for(uint64 j = 0; j < model->subsets[i].indices.size; j++)
							model->subsets[i].indices[j] = uint32(u16_array[j]);
						u16_array.clear();
					}

					if((*meshes)[i]["primitives"][0]["attributes"].find("TEXCOORD_0") != not_found<uint64>)
					{
						accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["attributes"]["TEXCOORD_0"].numeric_value)];
						if((*accessor)["type"].string_value == u8string("VEC2")
							&& uint32((*accessor)["componentType"].numeric_value) == component_type_float)
						{
							buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
							if(model->subsets[i].vertices.size != uint64((*accessor)["count"].numeric_value)) return false;
							vec2f_array.insert_default(0, model->subsets[i].vertices.size);
							f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
							f.read(model->subsets[i].vertices.size * sizeof(vector<float32, 2>), vec2f_array.addr);
							for(uint64 j = 0; j < model->subsets[i].vertices.size; j++)
								model->subsets[i].vertices[j].texture = vec2f_array[j];
							vec2f_array.clear();
						}
					}

					accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["attributes"]["NORMAL"].numeric_value)];
					if((*accessor)["type"].string_value == u8string("VEC3")
						&& uint32((*accessor)["componentType"].numeric_value) == component_type_float)
					{
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						if(model->subsets[i].vertices.size != uint64((*accessor)["count"].numeric_value)) return false;
						vec3f_array.insert_default(0, model->subsets[i].vertices.size);
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						f.read(model->subsets[i].vertices.size * sizeof(vector<float32, 3>), vec3f_array.addr);
						for(uint64 j = 0; j < model->subsets[i].vertices.size; j++)
							model->subsets[i].vertices[j].normal = vec3f_array[j];
						vec3f_array.clear();
					}
					
					if((*meshes)[i]["primitives"][0].find("material") != not_found<uint64>)
					{
						material = &json_data.root["materials"][uint64((*meshes)[i]["primitives"][0]["material"].numeric_value)];
						if((*material).find("normalTexture") != not_found<uint64>)
						{
							texture = &json_data.root["textures"][uint64((*material)["normalTexture"]["index"].numeric_value)];
							model->subsets[i].material.normal_texture = uint64((*texture)["source"].numeric_value);
						}
						else model->subsets[i].material.normal_texture = not_exists<uint64>;
						if((*material).find("pbrMetallicRoughness") != not_found<uint64>)
						{
							if((*material)["pbrMetallicRoughness"].find("baseColorTexture") != not_found<int64>)
							{
								texture = &json_data.root["textures"][uint64((*material)["pbrMetallicRoughness"]["baseColorTexture"]["index"].numeric_value)];
								model->subsets[i].material.base_color_texture = uint64((*texture)["source"].numeric_value);
							}
							else model->subsets[i].material.base_color_texture = not_exists<int64>;
							if((*material)["pbrMetallicRoughness"].find("metallicRoughnessTexture") != not_found<int64>)
							{
								texture = &json_data.root["textures"][uint64((*material)["pbrMetallicRoughness"]["metallicRoughnessTexture"]["index"].numeric_value)];
								model->subsets[i].material.metallic_texture = uint64((*texture)["source"].numeric_value);
							}
							else model->subsets[i].material.metallic_texture = not_exists<uint64>;
							if((*material)["pbrMetallicRoughness"].find("baseColorFactor") != not_found<uint64>)
							{
								model->subsets[i].material.base_color_factor.x = float32((*material)["pbrMetallicRoughness"]["baseColorFactor"][0].numeric_value);
								model->subsets[i].material.base_color_factor.y = float32((*material)["pbrMetallicRoughness"]["baseColorFactor"][1].numeric_value);
								model->subsets[i].material.base_color_factor.z = float32((*material)["pbrMetallicRoughness"]["baseColorFactor"][2].numeric_value);
								model->subsets[i].material.base_color_factor.w = float32((*material)["pbrMetallicRoughness"]["baseColorFactor"][3].numeric_value);
							}
							else
							{
								model->subsets[i].material.base_color_factor.x = 1.0f;
								model->subsets[i].material.base_color_factor.y = 1.0f;
								model->subsets[i].material.base_color_factor.z = 1.0f;
								model->subsets[i].material.base_color_factor.w = 1.0f;
							}
							if((*material)["pbrMetallicRoughness"].find("metallicFactor") != not_found<uint64>)
								model->subsets[i].material.metallic_factor = float32((*material)["pbrMetallicRoughness"]["metallicFactor"].numeric_value);
							else model->subsets[i].material.metallic_factor = 1.0f;
							if((*material)["pbrMetallicRoughness"].find("roughnessFactor") != not_found<uint64>)
								model->subsets[i].material.roughness_factor = float32((*material)["pbrMetallicRoughness"]["roughnessFactor"].numeric_value);
							else model->subsets[i].material.roughness_factor = 1.0f;
						}
					}

					if((*meshes)[i]["primitives"][0]["attributes"].find("JOINTS_0") != not_found<uint64>
						&& (*meshes)[i]["primitives"][0]["attributes"].find("WEIGHTS_0") != not_found<uint64>)
					{
						accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["attributes"]["JOINTS_0"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						if((*accessor)["type"].string_value == u8string("VEC4")
							&& uint32((*accessor)["componentType"].numeric_value) == component_type_ushort)
						{
							vec4u16_array.insert_default(0, model->subsets[i].vertices.size);
							f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
							f.read(model->subsets[i].vertices.size * sizeof(vector<uint16, 4>), vec4u16_array.addr);
						}
						accessor = &json_data.root["accessors"][uint64((*meshes)[i]["primitives"][0]["attributes"]["WEIGHTS_0"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						if((*accessor)["type"].string_value == u8string("VEC4")
							&& uint32((*accessor)["componentType"].numeric_value) == component_type_float)
						{
							vec4f_array.insert_default(0, model->subsets[i].vertices.size);
							f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
							f.read(model->subsets[i].vertices.size * sizeof(vector<float32, 4>), vec4f_array.addr);
						}
						if(vec4u16_array.size == model->subsets[i].vertices.size
							&& vec4f_array.size == model->subsets[i].vertices.size)
							for(uint64 j = 0; j < model->subsets[i].vertices.size; j++)
								for(uint32 k = 0; k < 4; k++)
								{
									if(vec4f_array[j].coord[k] < 0.001f)
									{
										model->subsets[i].vertices[j].joints.coord[k] = not_exists<int32>;
										break;
									}
									model->subsets[i].vertices[j].joints.coord[k] = int32(vec4u16_array[j].coord[k]);
									model->subsets[i].vertices[j].weights.coord[k] = vec4f_array[j].coord[k];
								}
						vec4u16_array.clear();
						vec4f_array.clear();
					}
					
					model->subsets[i].initialize_render_resources(device);
				}
				
				if(json_data.root.find("images") != not_found<uint64>)
				{
					array<byte> img;
					for(uint64 i = 0; i < json_data.root["images"].values.size; i++)
					{
						image = &json_data.root["images"][i];
						buffer_view = &json_data.root["bufferViews"][uint64((*image)["bufferView"].numeric_value)];
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						img.insert_default(0, uint64((*buffer_view)["byteLength"].numeric_value));
						f.read(img.size, img.addr);
						model->textures.push_default();
						os_create_texture_from_memory(
							device,
							img.addr,
							img.size,
							&model->textures.back().texture,
							&model->textures.back().shader_resource_view);
						img.clear();
					}
				}

				if(json_data.root.find("animations") == not_found<uint64>) return true;

				nodes = &json_data.root["nodes"];
				model->nodes.insert_default(0, (*nodes).values.size);
				for(uint64 i = 0; i < model->nodes.size; i++)
					model->nodes[i].parent = not_exists<int32>;
				for(uint64 i = 0; i < nodes->values.size; i++)
				{
					idx = (*nodes)[i].find("scale");
					if(idx != not_found<uint64>)
						model->nodes[i].offset = XMMatrixScaling(
							float32((*nodes)[i].values[idx].values[0].numeric_value),
							float32((*nodes)[i].values[idx].values[1].numeric_value),
							float32((*nodes)[i].values[idx].values[2].numeric_value));
					else model->nodes[i].offset = XMMatrixIdentity();
					idx = (*nodes)[i].find("rotation");
					if(idx != not_found<uint64>)
						model->nodes[i].offset *= XMMatrixRotationQuaternion(XMVectorSet(
							float32((*nodes)[i].values[idx].values[0].numeric_value),
							float32((*nodes)[i].values[idx].values[1].numeric_value),
							float32((*nodes)[i].values[idx].values[2].numeric_value),
							float32((*nodes)[i].values[idx].values[3].numeric_value)));
					idx = (*nodes)[i].find("translation");
					if(idx != not_found<uint64>)
						model->nodes[i].offset *= XMMatrixTranslation(
							float32((*nodes)[i].values[idx].values[0].numeric_value),
							float32((*nodes)[i].values[idx].values[1].numeric_value),
							float32((*nodes)[i].values[idx].values[2].numeric_value));
					idx = (*nodes)[i].find("children");
					if(idx != not_found<uint64>)
						for(uint64 j = 0; j < (*nodes)[i].values[idx].values.size; j++)
							model->nodes[uint64((*nodes)[i].values[idx].values[j].numeric_value)].parent = int32(i);
				}
				for(uint64 i = 0; i < nodes->values.size; i++)
					if(model->nodes[i].parent == not_exists<int32>)
						model->root_inverse_matrix = XMMatrixInverse(nullptr, model->nodes[i].offset);

				skin = &json_data.root["skins"][0];
				model->joints.insert_default(0, (*skin)["joints"].values.size);
				accessor = &json_data.root["accessors"][uint64((*skin)["inverseBindMatrices"].numeric_value)];
				buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
				f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
				array<XMMATRIX> inverse_bind_matrices;
				inverse_bind_matrices.insert_default(0, uint64((*accessor)["count"].numeric_value));
				f.read(inverse_bind_matrices.size * sizeof(XMMATRIX), inverse_bind_matrices.addr);
				for(uint64 i = 0; i < model->joints.size; i++)
				{
					model->joints[i].node = uint32((*skin)["joints"][i].numeric_value);
					model->joints[i].inverse_bind_matrix = inverse_bind_matrices[i];
				}

				animations = &json_data.root["animations"];
				for(uint64 i = 0; i < animations->values.size; i++)
				{
					model->animations.push_default();
					model->animations[i].total_animation_time = 0.0f;
					for(uint64 j = 0; j < (*animations)[i]["channels"].values.size; j++)
					{
						channel = &(*animations)[i]["channels"][j];
						sampler = &(*animations)[i]["samplers"][uint64((*channel)["sampler"].numeric_value)];
						model->animations[i].channels.push_default();

						model->animations[i].channels.back().node = uint32((*channel)["target"]["node"].numeric_value);

						if((*channel)["target"]["path"].string_value == "scale")
							model->animations[i].channels.back().target = model_channel_target::scaling;
						else if((*channel)["target"]["path"].string_value == "rotation")
							model->animations[i].channels.back().target = model_channel_target::rotating;
						else if((*channel)["target"]["path"].string_value == "translation")
							model->animations[i].channels.back().target = model_channel_target::translating;
						else model->animations[i].channels.back().target = model_channel_target::weighting;

						accessor = &json_data.root["accessors"][uint64((*sampler)["input"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						model->animations[i].channels.back().timestamps.insert_default(0, uint64((*accessor)["count"].numeric_value));
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						f.read(
							model->animations[i].channels.back().timestamps.size * sizeof(float32),
							model->animations[i].channels.back().timestamps.addr);

						model->animations[i].total_animation_time
							= max(model->animations[i].total_animation_time, model->animations[i].channels.back().timestamps.back());

						accessor = &json_data.root["accessors"][uint64((*sampler)["output"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						model->animations[i].channels.back().frames.insert_default(0, model->animations[i].channels.back().timestamps.size);
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						if(model->animations[i].channels.back().target == model_channel_target::rotating)
							f.read(
								model->animations[i].channels.back().frames.size * sizeof(vector<float32, 4>),
								model->animations[i].channels.back().frames.addr);
						else
						{
							vec3f_array.insert_default(0, model->animations[i].channels.back().frames.size);
							f.read(model->animations[i].channels.back().frames.size * sizeof(vector<float32, 3>), vec3f_array.addr);
							for(uint64 k = 0; k < model->animations[i].channels.back().frames.size; k++)
								model->animations[i].channels.back().frames[k]
									= vector<float32, 4>(vec3f_array[k].x, vec3f_array[k].y, vec3f_array[k].z, 0.0f);
							vec3f_array.clear();
						}
					}
				}
			}
			else return false;
		}
		catch(const std::exception &e)
		{
			return false;
		}
		return true;
	}
};


