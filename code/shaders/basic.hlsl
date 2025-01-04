//#include "shaderinterop_common.h"
#include "shadercommon.hlsl"

struct ModelVertex
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
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
    float3 texcoord = float3(f.texcoord.x, f.texcoord.y, f.texcoord.z);
    float4 diffuse = diffuseCubeMap.Sample(skyboxSampler, texcoord);
    diffuse.xyz = pow(diffuse.xyz, 1/ 2.2);
    return diffuse;
}
