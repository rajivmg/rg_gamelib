static char const* g_Simple2DShaderSrcCode = R"foo(
#include <metal_stdlib>
using namespace metal;

struct Vertex2D
{
    float2 pos;
    float2 texcoord;
    float4 color;
};

// struct FrameParams
// {
//     float4x4 orthoProjection;
//     constant Vertex2D* smallVertexBuffer;
// };

//--
struct VertexOut
{
    float4 position [[position]];
    half4 color;
};
//

//VertexOut vertex basicVertexShader(constant FrameParams& frameParams [[buffer(0)]],
//                                 uint vertexId [[vertex_id]], uint instanceId [[instance_id]])
VertexOut vertex simple2d_VS(constant float4x4& projection [[buffer(0)]],
                                   constant Vertex2D* smallVertexBuffer [[buffer(1)]],
                                   uint vertexId [[vertex_id]],
                                   uint instanceId [[instance_id]])
{
    VertexOut out;
    constant Vertex2D* v = &smallVertexBuffer[instanceId * 6 + vertexId];
    out.position = projection * float4(v->pos, -50.0, 1.0);
    out.color  = half4(v->color);
    return out;
}

fragment half4 simple2d_FS(VertexOut fragIn [[stage_in]])
{
    return fragIn.color;
}

)foo";
