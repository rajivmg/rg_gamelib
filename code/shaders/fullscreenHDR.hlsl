#include "common.hlsl"

struct VertexShaderOut
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD;
};

Texture2D<float4> srcTexture : register(t0, space0);
SamplerState pointSampler : register(s0, space0);

VertexShaderOut vsFullscreenHDRPassthrough(uint vertexID : SV_VERTEXID)
{
    //const float4 vertices[] = { float4(-1.0, -1.0, 0, 1.0), float4(3.0, -1.0, 0, 1.0), float4(-1.0, 3.0, 0, 1.0) };
    const float4 vertices[] = { float4(-1.0, 1.0, 0, 1.0), float4(-1.0, -3.0, 0, 1.0), float4(3.0, 1.0, 0, 1.0) };

    float4 position = vertices[vertexID];
    float2 texcoord = float2(0.5, -0.5) * position.xy + float2(0.5, 0.5);

    VertexShaderOut output;
    output.position = position;
    output.texcoord = texcoord;
    return output;
}

float4 fsReinhard(VertexShaderOut f) : SV_TARGET
{
    return srcTexture.Sample(pointSampler, f.texcoord);
}