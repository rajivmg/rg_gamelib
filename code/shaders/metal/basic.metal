#include <metal_stdlib>
using namespace metal;

struct BasicVertex
{
    float3 position;
    float4 color; // Make this half3
};

struct VertexOut
{
    float4 position [[position]];
    half4 color;
};

VertexOut vertex basicVertexShader(device const BasicVertex* vertexBuffer [[buffer(0)]],
                                 uint vertexId [[vertex_id]])
{
    VertexOut out;
    out.position = float4(vertexBuffer[vertexId].position, 1.0);
    out.color    = half4(vertexBuffer[vertexId].color);
    return out;
}

fragment half4 basicFragmentShader(VertexOut fragIn [[stage_in]])
{
    return fragIn.color;
}
