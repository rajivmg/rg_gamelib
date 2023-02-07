#include <metal_stdlib>
using namespace metal;

struct ImmVertex
{
    float3 position;
    float4 color;
};

struct VertexOut
{
    float4 position [[position]];
    half4 color;
};

VertexOut vertex immVS(device const ImmVertex* vertices [[buffer(0)]],
                       uint vertexId [[vertex_id]])
{
    VertexOut out;
    out.position = float4(vertices[vertexId].position, 1.0);
    out.color    = half4(vertices[vertexId].color);
    return out;
}

fragment half4 immFS(VertexOut fragIn [[stage_in]])
{
    return fragIn.color;
}
