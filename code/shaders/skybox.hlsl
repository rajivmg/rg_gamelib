#include "common.hlsl"

struct SkyboxVertex
{
    float3 position : POSITION;
};

struct SkyboxVertexShaderOut
{
    float4 position : SV_Position;
    float3 texcoord : TEXCOORD;
};

TextureCube<float4> diffuseCubeMap : register(t0, space0);
SamplerState skyboxSampler : register(s0, space0);

SkyboxVertexShaderOut vsSkybox(in SkyboxVertex v)
{
    float4 position = mul(cameraProjMatrix, mul(cameraViewRotOnlyMatrix, float4(v.position, 1.0)));
    SkyboxVertexShaderOut output;
    output.position = position.xyww;
    output.texcoord = v.position;
    return output;
}

float4 fsSkybox(in SkyboxVertexShaderOut f) : SV_TARGET
{
    float4 diffuse = diffuseCubeMap.Sample(skyboxSampler, f.texcoord);
    diffuse.xyz = pow(diffuse.xyz, 1/ 2.2);
    return diffuse;
}
