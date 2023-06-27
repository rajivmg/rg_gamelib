#include <metal_stdlib>
using namespace metal;

struct Obj2HeaderModelVertex
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VertexShaderOut
{
    float4 position : SV_Position;
    float3 normal;
    float2 texcoord;
    nointerpolation uint instanceId;
};

struct InstanceParams
{
    float4x4 worldXform;
    float4x4 invTposWorldXform;
};

struct CameraParams
{
    float4x4 projectionPerspective;
    float4x4 viewCamera;
};

cbuffer CombinedCB : register(b0)
{
    CameraParams cameraParams;
    InstanceParams instanceParams;
};

/*struct BindlessResources
{
    array<texture2d<float>, 1000> textures2d [[id(0)]];
};*/

float attenuate(float distance, float range, float3 distibutionCoeff)
{
    float a = 1.0f / (distibutionCoeff.x * distance * distance + distibutionCoeff.y * distance + distibutionCoeff.z);
    return step(distance, range) * saturate(a);
}

VertexShaderOut vsPrincipledBrdf(in Obj2HeaderModelVertex vertexInput,
                                 uint instanceId : SV_InstanceID)
{
    InstanceParams& instance = instanceParams[instanceId];
    float3 worldPos = (instance.worldXform * float4(in.position, 1.0)).xyz;
    float3 normal = normalize((instance.invTposWorldXform * float4(in.normal, 1.0)).xyz);

    float3 distributionCoeff(2.0, 3.0, 0.1);
    float3 lightPos(0.0f, 1.0f, 1.2f);
    float atten = attenuate(distance(worldPos, lightPos), 2.2, distributionCoeff);
    float3 lightDir = normalize(lightPos - worldPos);

    float nDotL = max(0.0, dot(normal, lightDir));
    float lightCoeff = atten * nDotL;

    VertexShaderOut out;
    out.position = cameraParams.projectionPerspective * cameraParams.viewCamera * float4(worldPos, 1.0);
    out.normal = normal;
    out.texcoord = in.texcoord;
    out.vertexLightCoeff = lightCoeff;
    out.instanceId = instanceId;
    return out;
}

half4 fsPrincipledBrdf(constant InstanceParams* instanceParams  [[buffer(1)]],
                                VertexShaderOut in [[stage_in]])
{
    half4 color(1.0, 0.1, 0.1, 1.0);
    //float4 color = float4(in.normal, 1.0);
    //return half4(color + (float4(1.0, 1.0, 1.0, 1.0) * in.vertexLightCoeff));
    return half4(color.rgb * (half)in.vertexLightCoeff, 1.0h);
}

)foo";
