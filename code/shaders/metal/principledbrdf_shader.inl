static char const* g_PrincipledBrdfShaderSrcCode = R"foo(
#include <metal_stdlib>
using namespace metal;

struct Obj2HeaderModelVertex
{
    float3 position     [[attribute(0)]];
    float3 normal       [[attribute(1)]];
    float2 texcoord     [[attribute(2)]];
};

struct VertexShaderOut
{
    float4 position     [[position]];
    float3 normal;
    float2 texcoord;
    float  vertexLightCoeff;
    uint instanceId         [[flat]];
};

struct InstanceParams
{
    float4x4 modelXform;
    float4x4 invTposModelXform;
};

struct CameraParams
{
    float4x4 projectionPerspective;
    float4x4 viewCamera;
};

struct BindlessResources
{
    array<texture2d<float>, 1000> textures2d [[id(0)]];
};

float attenuate(float distance, float range, float3 distibutionCoeff)
{
    float a = 1.0f / (distibutionCoeff.x * distance * distance + distibutionCoeff.y * distance + distibutionCoeff.z);
    return step(distance, range) * saturate(a);
}

VertexShaderOut vertex vsPrincipledBrdf(constant CameraParams& cameraParams [[buffer(0)]],
                                        constant InstanceParams* instanceParams  [[buffer(1)]],
                                        Obj2HeaderModelVertex in                 [[stage_in]],
                                        uint vertexId      [[vertex_id]],
                                        uint instanceId    [[instance_id]])
{
    InstanceParams* instance = &instanceParams[instanceId];
    float3 worldPos = instance.worldXform * float4(in.position, 1.0);
    float3 normal = normalize(invTposModelXform * float4(in.normal, 1.0));

    float3 distributionCoeff(2.0, 1.5, 1.0);
    float3 lightPos(0.4f, 1.0f, 1.2f);
    float atten = attenuate(distance(worldPos, lightPos), 0.2, distributionCoeff);
    float3 lightDir = normalize(lightPos - worldPos);

    float nDotL = max(0.0, dot(normal, lightDir));
    float lightCoeff = atten * nDotL;

    VertexShaderOut out;
    out.position = cameraParams.projectionPerspective * cameraParams.viewCamera * worldPos;
    out.normal = normal;
    out.texcoord = in.texcoord;
    out.vertexLightCoeff = lightCoeff;
    out.instanceId = instanceId;
    return out;
}

fragment half4 fsPrincipledBrdf(constant InstanceParams* instanceParams  [[buffer(1)]],
                                VertexShaderOut in [[stage_in]])
{
    float4 color = float4(0.5, 0.3, 1.0, 1.0);
    //float4 color = float4(in.normal, 1.0);
    return half4(color * in.vertexLightCoeff);
}

)foo";
