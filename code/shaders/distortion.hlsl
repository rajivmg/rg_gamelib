#include <shadercommon.hlsl>

struct DistortionParams
{
    float   time;
    float   pad_1;
    float2  pad_2;
};

ConstantBuffer<DistortionParams> distortionParams : register(b0, space0);
Texture2D<float4> backgroundTexture;
SamplerState distortionSampler : register(s0, space0);

float2 rotate(float2 uv, float range)
{
    // range - 0 to 1, where 1 means one full revolution
    float angle = range;// * 6.28;
    float c = cos(angle);
    float s = sin(angle);
    float2 tp = uv - 0.5f;
    return float2(tp.x * c - tp.y * s, tp.x * s + tp.y * c) + 0.5f;
}

float4 fsDistortion(in VS_OUT fin) : SV_TARGET
{
    float2 newUV = rotate(fin.texcoord, distortionParams.time);
    float4 tex = backgroundTexture.Sample(distortionSampler, newUV);
    return tex;//float4(, 0, 1.0f);
}
