#include "shaderinterop_common.h"

#define MAX_INSTANCES 256

struct Obj2HeaderModelVertex
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal   : NORMAL;
};

struct InstanceParams
{
    float4x4 worldXform[MAX_INSTANCES];
    float4x4 invTposWorldXform[MAX_INSTANCES];
};

ConstantBuffer<InstanceParams> instanceParams : register(b1, space0);
Texture2D<float4> diffuseTexMap;
TextureCube<float4> irradianceMap : register(t0, space0);
SamplerState irradianceSampler : register(s0, space0);

//SamplerState simpleSampler : register(s0, space0);

float attenuate(float distance, float range, float3 distibutionCoeff)
{
    float a = 1.0f / (distibutionCoeff.x * distance * distance + distibutionCoeff.y * distance + distibutionCoeff.z);
    return step(distance, range) * saturate(a);
}

VS_OUT vsPbr(in Obj2HeaderModelVertex v, uint instanceID : SV_InstanceID)
{
    float3 worldPos = mul(instanceParams.worldXform[instanceID], float4(v.position, 1.0)).xyz;
    float3 normal = normalize(mul(instanceParams.invTposWorldXform[instanceID], float4(v.normal, 1.0)).xyz);

    float3 distributionCoeff = float3(2.0, 3.0, 0.1);
    float3 lightPos = float3(0.0f, 1.0f, 1.2f);
    float atten = attenuate(distance(worldPos, lightPos), 2.2, distributionCoeff);
    float3 lightDir = normalize(lightPos - worldPos);

    float nDotL = max(0.0, dot(normal, lightDir));
    float lightCoeff = atten * nDotL;

    VS_OUT output;
    output.position = mul(cameraProjMatrix, mul(cameraViewMatrix, float4(worldPos, 1.0)));
    output.normal = normal;
    output.texcoord = v.texcoord;
    output.instanceID = instanceID;
    return output;
}

float4 fsPbr(in VS_OUT f) : SV_TARGET
{
    //half4 color = half4(1.0, 0.1, 0.1, 1.0);
    float4 diffColor = diffuseTexMap.Sample(irradianceSampler, f.texcoord);
    half4 irradiance = irradianceMap.Sample(irradianceSampler, f.normal);
    return diffColor * irradiance;
    //float4 color = float4(in.normal, 1.0);
    //return half4(color + (float4(1.0, 1.0, 1.0, 1.0) * in.vertexLightCoeff));
    //return half4(color.rgb * (half)(f.vertexLightCoeff + 0.168f), 1.0h);
}
