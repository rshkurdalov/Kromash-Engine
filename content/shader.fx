cbuffer constant_buffer
{
	uint width;
	uint height;
	float4x4 wvp;
	float4x4 world;
	float4 clr;
	uint radius;
};

Texture2D texture1;
Texture2D<float4> texture2;
SamplerState obj_sampler_state;
TextureCube sky_map;

struct vs_output
{
	float4 pos : SV_POSITION;
	float2 texture_coord : TEXCOORD;
	float3 normal : NORMAL;
};

struct skymap_vs_output
{
	float4 pos : SV_POSITION;
	float3 texture_coord : TEXCOORD;
};

vs_output vertex_shader(float4 pos : POSITION, float2 texture_coord : TEXCOORD, float3 normal : NORMAL)
{
	vs_output output;
	output.pos = mul(pos, wvp);
	output.texture_coord = texture_coord;
	output.normal = mul(normal, world);
	return output;
}

float4 pixel_shader(vs_output input) : SV_TARGET
{
	float4 color = texture1.Sample(obj_sampler_state, input.texture_coord);
	if(color.a == 0.0) clip(-1.0);
    return color;
}

vs_output ui_vertex_shader(float4 pos : POSITION, float2 texture_coord : TEXCOORD)
{
    vs_output output;
    output.pos = pos;
    output.texture_coord = texture_coord;
    return output;
}

float4 ui_pixel_shader(vs_output input) : SV_TARGET
{
	float4 color = texture1.Sample(obj_sampler_state, input.texture_coord);
	if(color.a == 0.0) clip(-1.0);
    return color;
}

skymap_vs_output skymap_vertex_shader(float3 pos : POSITION, float2 texture_coord : TEXCOORD, float3 normal : NORMAL)
{
	skymap_vs_output output;
	output.pos = mul(float4(pos, 1.0f), wvp).xyww;
	output.texture_coord = pos;
	return output;
}

float4 skymap_pixel_shader(skymap_vs_output input) : SV_Target
{
	return sky_map.Sample(obj_sampler_state, input.texture_coord);
}

vs_output hit_test_vertex_shader(float4 pos : POSITION, float2 texture_coord : TEXCOORD, float3 normal : NORMAL)
{
	vs_output output;
	output.pos = mul(pos, wvp);
	return output;
}

float4 hit_test_pixel_shader(vs_output input) : SV_TARGET
{
    return float4(1.0, 1.0, 1.0, 1.0);
}

vs_output outline_vertex_shader(float4 pos : POSITION, float2 texture_coord : TEXCOORD, float3 normal : NORMAL)
{
	vs_output output;
	output.pos = pos;
	return output;
}

float4 outline_pixel_shader(vs_output input) : SV_TARGET
{
	float4 color = texture2.Sample(obj_sampler_state, float2(input.pos.x / width, input.pos.y / height));
	if(color.r > 0.0 || radius == 0)
	{
		clip(-1.0);
		return clr;
	}
	int2 v = int2(input.pos.x - radius, input.pos.y - radius),
		q = int2(input.pos.x + radius, input.pos.y + radius);
	uint hit_test = 0;
	int y = v.y;
	float r2 = radius * radius;
	while(v.x <= q.x)
	{
		if((input.pos.x - v.x) * (input.pos.x - v.x) + (input.pos.y - y) * (input.pos.y - y) <= r2
			&& v.x >= 0 && v.x < width && y >= 0 && y < height)
		{
			color = texture2.Load(uint3(v.x, y, 0));
			if(color.r > 0.0) hit_test = 1;
		}
		y++;
		if(y > q.y)
		{
			v.x++;
			y = v.y;
		}
	}
	if(!hit_test) clip(-1.0);
	return clr;
}
