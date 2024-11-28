cbuffer constant_buffer
{
	float4x4 wvp;
	float4x4 world;
	uint width;
	uint height;
	uint animated;
	uint radius;
	float4 clr;
    uint manual_uv;
	float begin_x;
	float begin_z;
	float size_x;
	float size_z;
    uint pbr;
    uint2 padding1;

    float3 light_dir;
    uint padding3;
    float3 light_color;
    uint padding4;
    
    float3 camera;
    uint padding5;
    float4 baseColorFactor;
    float metallic_factor;
    float roughness_factor;
};

cbuffer constant_buffer_joints
{
	float4x4 final_transform[250];
};

Texture2D texture1 : register(t0);
Texture2D texture2 : register(t1);
Texture2D texture3 : register(t2);
TextureCube sky_map : register(t4);
SamplerState obj_sampler_state;

struct vs_output
{
	float4 pos : SV_POSITION;
    float3 poswithoutw : POSITION;
	float2 texture_coord : TEXCOORD;
	float3 normal : NORMAL;
};

struct skymap_vs_output
{
	float4 pos : SV_POSITION;
	float3 texture_coord : TEXCOORD;
};

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
	float4 color = texture1.Sample(obj_sampler_state, float2(input.pos.x / width, input.pos.y / height));
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
			color = texture1.Load(uint3(v.x, y, 0));
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

vs_output vertex_shader(float4 pos : POSITION, float2 texture_coord : TEXCOORD, float3 normal : NORMAL, int4 joints : JOINTS, float4 weights : WEIGHTS)
{
	vs_output output;
	if(animated)
	{
		output.pos = float4(0.0, 0.0, 0.0, 0.0);
        output.normal = float3(0.0, 0.0, 0.0);
		for(uint i = 0; i < 4; i++)
		{
			if(joints[i] == -1) break;
			output.pos += mul(pos, final_transform[joints[i]]) * weights[i];
            output.normal += mul(normal, final_transform[joints[i]]) * weights[i];
		}
        float4 wpos = mul(output.pos, world);
        output.poswithoutw = float3(wpos.xyz) / wpos.w;
		output.pos = mul(output.pos, wvp);
		output.normal = normalize(mul(output.normal, world));
	}
	else
	{
        float4 wpos = mul(pos, world);
        output.poswithoutw = float3(wpos.xyz) / wpos.w;
		output.pos = mul(pos, wvp);
		output.normal = normalize(mul(normal, world));
	}
    if(manual_uv)
    {
        output.texture_coord = float2((pos.x - begin_x) / size_x, (pos.z - begin_z) / size_z);
    }
    else
    {
        output.texture_coord = texture_coord;
    }
	return output;
}

//
// This fragment shader defines a reference implementation for Physically Based Shading of
// a microfacet surface material defined by a glTF model.
//
// References:
// [1] Real Shading in Unreal Engine 4
//     http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// [2] Physically Based Shading at Disney
//     http://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// [3] README.md - Environment Maps
//     https://github.com/KhronosGroup/glTF-WebGL-PBR/#environment-maps
// [4] "An Inexpensive BRDF Model for Physically based Rendering" by Christophe Schlick
//     https://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf

#define NORMALS
#define UV
#define HAS_NORMALS
#define USE_IBL
#define USE_TEX_LOD
#define HAS_NORMALMAP

#ifdef HAS_NORMALS
#ifdef HAS_TANGENTS
varying mat3 v_TBN;
#else
#endif
#endif

struct PBRInfo
{
    float NdotL; // cos angle between normal and light direction
    float NdotV; // cos angle between normal and view direction
    float NdotH; // cos angle between normal and half vector
    float LdotH; // cos angle between light direction and half vector
    float VdotH; // cos angle between view direction and half vector
    float perceptualRoughness; // roughness value, as authored by the model creator (input to shader)
    float metalness; // metallic value at the surface
    float3 reflectance0; // full reflectance color (normal incidence angle)
    float3 reflectance90; // reflectance color at grazing angle
    float alphaRoughness; // roughness mapped to a more linear change in the roughness (proposed by [2])
    float3 diffuseColor; // color contribution from diffuse lighting
    float3 specularColor; // color contribution from specular lighting
};

static const float M_PI = 3.141592653589793;
static const float c_MinRoughness = 0.04;

float4 SRGBtoLINEAR(float4 srgbIn)
{
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
    float3 linOut = pow(srgbIn.xyz,float3(2.2, 2.2, 2.2));
#else //SRGB_FAST_APPROXIMATION
    float3 bLess = step(float3(0.04045, 0.04045, 0.04045), srgbIn.xyz);
    float3 linOut = lerp(srgbIn.xyz / float3(12.92, 12.92, 12.92), pow((srgbIn.xyz + float3(0.055, 0.055, 0.055)) / float3(1.055, 1.055, 1.055), float3(2.4, 2.4, 2.4)), bLess);
#endif //SRGB_FAST_APPROXIMATION
    return float4(linOut,srgbIn.w);;
#else //MANUAL_SRGB
    return srgbIn;
#endif //MANUAL_SRGB
}

float3 getNormal(float3 position, float3 normal, float2 uv)
{
    // Retrieve the tangent space matrix
#ifndef HAS_TANGENTS
    float3 pos_dx = ddx(position);
    float3 pos_dy = ddy(position);
    float3 tex_dx = ddx(float3(uv, 0.0));
    float3 tex_dy = ddy(float3(uv, 0.0));
    float3 t = (tex_dy.y * pos_dx - tex_dx.y * pos_dy) / (tex_dx.x * tex_dy.y - tex_dy.x * tex_dx.y);

#ifdef HAS_NORMALS
    float3 ng = normalize(normal);
#else
    float3 ng = cross(pos_dx, pos_dy);
#endif

    t = normalize(t - ng * dot(ng, t));
    float3 b = normalize(cross(ng, t));
    row_major float3x3 tbn = float3x3(t, b, ng);

#else // HAS_TANGENTS
    mat3 tbn = v_TBN;
#endif

#ifdef HAS_NORMALMAP
    float3 n = texture3.Sample(obj_sampler_state, uv).rgb;
    float normalScale = 1.0;
    // Need to check the multiplication is equivalent..
    n = normalize(mul(((2.0 * n - 1.0) * float3(normalScale, normalScale, 1.0)), tbn));
#else
    float3 n = tbn[2].xyz;
#endif

    return n;
}

float3 diffuse(PBRInfo pbrInputs)
{
    return pbrInputs.diffuseColor / M_PI;
}

float3 specularReflection(PBRInfo pbrInputs)
{
    return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
}

float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.alphaRoughness * pbrInputs.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (M_PI * f * f);
}

float4 pixel_shader(vs_output input) : SV_TARGET
{
    if(!pbr)
    {
	    float4 color = texture1.Sample(obj_sampler_state, input.texture_coord);
	    if(color.a == 0.0) clip(-1.0);
        return color;
    }

    float perceptualRoughness = roughness_factor;
    float metallic = metallic_factor;

    float4 mrSample = texture2.Sample(obj_sampler_state, input.texture_coord);
    perceptualRoughness = mrSample.g * perceptualRoughness;
    metallic = mrSample.b * metallic;

    perceptualRoughness = clamp(perceptualRoughness, c_MinRoughness, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    float alphaRoughness = perceptualRoughness * perceptualRoughness;

//#ifdef HAS_BASECOLORMAP
    float4 baseColor = SRGBtoLINEAR(texture1.Sample(obj_sampler_state, input.texture_coord)) * baseColorFactor;
    //float4 baseColor = baseColorFactor;

    float3 f0 = float3(0.04, 0.04, 0.04);
    float3 diffuseColor = baseColor.rgb * (float3(1.0, 1.0, 1.0) - f0);

    diffuseColor *= 1.0 - metallic;

    float3 specularColor = lerp(f0, baseColor.rgb, metallic);

    // Compute reflectance.
    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    // For typical incident reflectance range (between 4% to 100%) set the grazing reflectance to 100% for typical fresnel effect.
    // For very low reflectance range on highly diffuse objects (below 4%), incrementally reduce grazing reflecance to 0%.
    float reflectance90 = clamp(reflectance * 25.0, 0.0, 1.0);
    float3 specularEnvironmentR0 = specularColor.rgb;
    float3 specularEnvironmentR90 = float3(1.0, 1.0, 1.0) * reflectance90;

    float3 n = getNormal(input.poswithoutw, input.normal, input.texture_coord); // normal at surface point
    float3 v = normalize(camera - input.poswithoutw); // Vector from surface point to camera
    
    float3 l = normalize(light_dir); // Vector from surface point to light
    float3 h = normalize(l + v); // Half vector between both l and v
    float3 reflection = -normalize(reflect(v, n));

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = abs(dot(n, v)) + 0.001;
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    PBRInfo pbrInputs;
    pbrInputs.NdotL = NdotL;
    pbrInputs.NdotV = NdotV;
    pbrInputs.NdotH = NdotH;
    pbrInputs.LdotH = LdotH;
    pbrInputs.VdotH = VdotH;
    pbrInputs.perceptualRoughness = perceptualRoughness;
    pbrInputs.metalness = metallic;
    pbrInputs.reflectance0 = specularEnvironmentR0;
    pbrInputs.reflectance90 = specularEnvironmentR90;
    pbrInputs.alphaRoughness = alphaRoughness;
    pbrInputs.diffuseColor = diffuseColor;
    pbrInputs.specularColor = specularColor;

    float3 F = specularReflection(pbrInputs);
    
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    float3 diffuseContrib = (1.0 - F) * diffuse(pbrInputs);
    float3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    float3 color = NdotL * light_color * (diffuseContrib + specContrib);

    color = color * 2.0;

    /*color = lerp(color, F, scaleFGDSpec.x);
    color = lerp(color, float3(G, G, G), scaleFGDSpec.y);
    color = lerp(color, float3(D, D, D), scaleFGDSpec.z);
    color = lerp(color, specContrib, scaleFGDSpec.w);
    color = lerp(color, diffuseContrib, scaleDiffBaseMR.x);
    color = lerp(color, baseColor.rgb, scaleDiffBaseMR.y);
    color = lerp(color, float3(metallic, metallic, metallic), scaleDiffBaseMR.z);
    color = lerp(color, float3(perceptualRoughness, perceptualRoughness, perceptualRoughness), scaleDiffBaseMR.w);*/

    return float4(color, 1.0);
}