static char const* g_Simple2DShaderSrcCode = R"foo(
#include <metal_stdlib>
using namespace metal;

struct Vertex2D
{
    float2 pos;
    float2 texcoord;
    float4 color;
};

struct VertexOut
{
    float4 position [[position]];
    float2 texcoord;
    half4 color;
    uint instanceId [[flat]];
};

struct SimpleInstanceParams
{
    uint texID;
};

struct FrameResources
{
    array<texture2d<float>, 99999> textures2d [[id(0)]];
};

struct Camera
{
    float4x4 projection;
    float4x4 view;
};

struct FrameConstBuffer
{
    constant Camera* dummy0 [[id(0)]];
    constant Camera* dummy1 [[id(1)]];
    //constant Camera* dummy2 [[id(2)]];
    constant Camera* camera [[id(10)]];
};

VertexOut vertex simple2d_VS(constant FrameConstBuffer* frameConstBuffer [[buffer(0)]],
                             constant Vertex2D* vertexBuffer [[buffer(1)]],
                             constant SimpleInstanceParams* instanceParams [[buffer(2)]],
                             uint vertexId [[vertex_id]],
                             uint instanceId [[instance_id]])
{
    VertexOut out;
    constant Vertex2D* v = &vertexBuffer[instanceId * 6 + vertexId];
    out.position = frameConstBuffer->camera->projection * frameConstBuffer->camera->view * float4(v->pos, 1.0, 1.0);
    out.texcoord = v->texcoord;
    out.color  = half4(v->color);
    out.instanceId = instanceId;
    return out;
}

fragment half4 simple2d_FS(VertexOut fragIn [[stage_in]],
                           device FrameResources& frameResources [[buffer(7)]],
                           constant SimpleInstanceParams* instanceParams [[buffer(4)]])
{
    //return half4(1.0, 0.0, 1.0, 1.0);
    constexpr sampler pointSampler(filter::nearest);
    float4 color = frameResources.textures2d[instanceParams[fragIn.instanceId].texID].sample(pointSampler, fragIn.texcoord);
    return half4(color);
}

)foo";
