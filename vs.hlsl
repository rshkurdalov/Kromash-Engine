cbuffer cbPerObject
{
    float4x4 WVP;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 TexCoord : TEXCOORD;
};

VS_OUTPUT VS(float4 inPos : POSITION, float2 inTexCoord : TEXCOORD)
{
    VS_OUTPUT output;

    output.Pos = inPos;
    output.TexCoord = inTexCoord;

    return output;
}
