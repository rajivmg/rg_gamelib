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
    uint instanceId         [[flat]];
};

struct InstanceParams
{
    float4x4 modelXform;
    uint texID;
};

struct CameraParams
{
    float4x4 projectionPerspective;
    float4x4 viewCamera;
};

struct DescSpace0
{
    constant CommonFrameParams* commonFrameParams [[id(0)]];
};

struct BindlessResources
{
    array<texture2d<float>, 1000> textures2d [[id(0)]];
};

VertexShaderOut vertex vsPrincipledBrdf(constant CameraParams& cameraParams [[buffer(0)]],
                                        constant InstanceParams* instanceParams  [[buffer(1)]],
                                        Obj2HeaderModelVertex in                 [[stage_in]],
                                        uint vertexId      [[vertex_id]],
                                        uint instanceId    [[instance_id]])
{
    //InstanceParams* instance = &instanceParams[instanceId];
    VertexShaderOut out;
    out.position = cameraParams->projectionPerspective * cameraParams->viewCamera * float4(in.position, 1.0);
    out.normal = in.normal; // TODO: transform 
    out.texcoord = in.texcoord;
    out.instanceId = instanceId;
    return out;
}

fragment half4 fsPrincipledBrdf(constant InstanceParams* instanceParams  [[buffer(1)]],
                                VertexShaderOut in [[stage_in]])
{
    float4 color = float4(1.0, 0.0, 1.0, 1.0);
    float4 color = float4(in.normal, 1.0);
    return half4(color);
}

)foo";
