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

// struct FrameParams
// {
//     float4x4 orthoProjection;
//     constant Vertex2D* smallVertexBuffer;
// };

struct InstanceParams
{
    uint texID;
};

struct FrameResources
{
    array<texture2d<float>, 100000> textures2d [[id(0)]];
};


VertexOut vertex simple2d_VS(constant float4x4& projection [[buffer(0)]],
                             constant Vertex2D* smallVertexBuffer [[buffer(1)]],
                             constant InstanceParams* instanceParams [[buffer(2)]],
                             uint vertexId [[vertex_id]],
                             uint instanceId [[instance_id]])
{
    VertexOut out;
    constant Vertex2D* v = &smallVertexBuffer[instanceId * 6 + vertexId];
    out.position = projection * float4(v->pos, 0, 1.0);
    out.position.z = 0.5;
    out.texcoord = v->texcoord;
    out.color  = half4(v->color);
    out.instanceId = instanceId;
    return out;
}

fragment half4 simple2d_FS(VertexOut fragIn [[stage_in]],
                           device FrameResources& frameResources [[buffer(3)]])
{
    //return half4(1.0, 0.0, 1.0, 1.0);
    //return fragIn.color;
    constexpr sampler pointSampler(filter::nearest);
    float4 color = frameResources.textures2d[fragIn.instanceId].sample(pointSampler, fragIn.texcoord);
    return half4(color);
}

)foo";
