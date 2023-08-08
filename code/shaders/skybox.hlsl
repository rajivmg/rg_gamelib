#include "common.hlsl"

struct SkyboxVertex
{
    float3 position : POSITION;
};

struct VertexShaderOut
{
    float4 position : SV_Position;
    float3 texcoord : TEXCOORD;
};

TextureCube<float4> diffuseCubeMap : register(t0, space0);
SamplerState cubeMapSampler : register(s0, space0);

VertexShaderOut vsSkybox(in SkyboxVertex v)
{
    float4 position = mul(cameraProjMatrix, mul(cameraViewRotOnlyMatrix, float4(v.position, 1.0)));
    VertexShaderOut output;
    output.position = position.xyww;
    output.texcoord = v.position;
    return output;
}

float4 fsSkybox(in VertexShaderOut f) : SV_TARGET
{
    float4 diffuse = diffuseCubeMap.Sample(cubeMapSampler, f.texcoord);
    return diffuse;
}