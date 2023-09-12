#include "common.hlsl"

Texture2D<float4> srcTexture : register(t0, space0);
SamplerState pointSampler : register(s0, space0);

float3 reinhard(float3 c)
{
    return c / (1.0 + c);
}

float4 fsReinhard(VS_OUT f) : SV_TARGET
{
    float4 color = srcTexture.Sample(pointSampler, f.texcoord);
    //color.xyz = reinhard(color.xyz);

    //color.xyz = pow(color.xyz, float3(0.454545, 0.454545, 0.454545));

    return color;
}
