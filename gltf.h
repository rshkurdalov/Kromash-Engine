#pragma once
#include "json.h"
#include "world_object.h"

struct gltf
{
	u16string filename;
	world_model *model;
	file f;
	json<char8> json_data;
	uint64 buffer_begin;
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
			while(filename_idx != not_exists && f.filename[filename_idx] != u'.')
				filename_idx--;
			if(filename_idx == not_exists) return false;
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
						model->subsets[i].positions.insert_default(0, model->subsets[i].vertices.size);
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

					material = &json_data.root["materials"][uint64((*meshes)[i]["primitives"][0]["material"].numeric_value)];
					texture = &json_data.root["textures"][uint64((*material)["normalTexture"]["index"].numeric_value)];
					image = &json_data.root["images"][uint64((*texture)["source"].numeric_value)];
					buffer_view = &json_data.root["bufferViews"][uint64((*image)["bufferView"].numeric_value)];
					f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
					model->subsets[i].material.image.insert_default(0, uint64((*buffer_view)["byteLength"].numeric_value));
					f.read(model->subsets[i].material.image.size, model->subsets[i].material.image.addr);

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
					
					if(vec4u16_array.size != 0 && vec4f_array.size != 0)
					{
						model_weight mw;
						for(uint64 j = 0; j < model->subsets[i].vertices.size; j++)
						{
							model->subsets[i].vertices[j].weight_idx = uint32(model->subsets[i].weights.size);
							model->subsets[i].vertices[j].weight_count = 0;
							for(uint32 k = 0; k < 4; k++)
							{
								if(vec4f_array[j].coord[k] < 0.001f) break;
								mw.joint_idx = uint32(vec4u16_array[j].coord[k]);
								mw.bias = vec4f_array[j].coord[k];
								model->subsets[i].weights.push(mw);
								model->subsets[i].vertices[j].weight_count++;
							}
						}
					}
					vec4u16_array.clear();
					vec4f_array.clear();
				}
				animations = &json_data.root["animations"];
				nodes = &json_data.root["nodes"];
				model->joints.insert_default(0, nodes->values.size);
				for(uint64 i = 0; i < animations->values.size; i++)
				{
					model->animations.push_default();
					model->animations[i].total_animation_time = 0.0f;
					for(uint64 j = 0; j < (*animations)[i]["channels"].values.size; j++)
					{
						channel = &(*animations)[i]["channels"][j];
						sampler = &(*animations)[i]["samplers"][uint64((*channel)["sampler"].numeric_value)];
						model->animations[i].frame_skeleton.push_default();

						model->animations[i].frame_skeleton.back().joint_idx = uint32((*channel)["target"]["node"].numeric_value);

						if((*channel)["target"]["path"].string_value == "scale")
							model->animations[i].frame_skeleton.back().frame_type = joint_frame_transform_type::scaling;
						else if((*channel)["target"]["path"].string_value == "rotation")
							model->animations[i].frame_skeleton.back().frame_type = joint_frame_transform_type::rotating;
						else if((*channel)["target"]["path"].string_value == "translation")
							model->animations[i].frame_skeleton.back().frame_type = joint_frame_transform_type::translating;
						else model->animations[i].frame_skeleton.back().frame_type = joint_frame_transform_type::weighting;

						accessor = &json_data.root["accessors"][uint64((*sampler)["input"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						model->animations[i].frame_skeleton.back().timestamps.insert_default(0, uint64((*accessor)["count"].numeric_value));
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						f.read(
							model->animations[i].frame_skeleton.back().timestamps.size * sizeof(float32),
							model->animations[i].frame_skeleton.back().timestamps.addr);

						model->animations[i].total_animation_time
							= max(model->animations[i].total_animation_time, model->animations[i].frame_skeleton.back().timestamps.back());

						accessor = &json_data.root["accessors"][uint64((*sampler)["output"].numeric_value)];
						buffer_view = &json_data.root["bufferViews"][uint64((*accessor)["bufferView"].numeric_value)];
						model->animations[i].frame_skeleton.back().frames.insert_default(0, model->animations[i].frame_skeleton.back().timestamps.size);
						f.position = buffer_begin + uint64((*buffer_view)["byteOffset"].numeric_value);
						if(model->animations[i].frame_skeleton.back().frame_type == joint_frame_transform_type::rotating)
							f.read(
								model->animations[i].frame_skeleton.back().frames.size * sizeof(vector<float32, 4>),
								model->animations[i].frame_skeleton.back().frames.addr);
						else
						{
							vec3f_array.insert_default(0, model->animations[i].frame_skeleton.back().frames.size);
							f.read(model->animations[i].frame_skeleton.back().frames.size * sizeof(vector<float32, 3>), vec3f_array.addr);
							for(uint64 k = 0; k < model->animations[i].frame_skeleton.back().frames.size; k++)
								model->animations[i].frame_skeleton.back().frames[k]
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
			f.close();
			return false;
		}
		f.close();
		return true;
	}
};


