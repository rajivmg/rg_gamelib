static char const* g_ImmShaderSrcCode = R"foo(
#include <metal_stdlib>
using namespace metal;

struct ImmVertex
{
    float3 position;
    float2 texcoord;
    float4 color;
};

struct VertexOut
{
    float4 position [[position]];
    float2 texcoord;
    half4 color;
};

struct ImmParams
{
    float4 viewportSize; // xy: size, zw: 1/size
    float2 cursorPosInPixels;

    texture2d<half> texMap;
    sampler texMapSampler;
};

VertexOut vertex immVS(device const ImmVertex* vertices [[buffer(0)]],
                       uint vertexId [[vertex_id]])
{
    VertexOut out;
    out.position = float4(vertices[vertexId].position, 1.0);
    out.texcoord = vertices[vertexId].texcoord;
    out.color    = half4(vertices[vertexId].color);
    return out;
}

fragment half4 immFS(VertexOut fragIn [[stage_in]],
                     texture2d<half, access::sample> dummyTexMap [[texture(0)]])
{
#if IMM_FORCE_RED
    return half4(1.0, 0.0, 0.0, 1.0);
#elif IMM_FORCE_UV_OUTPUT
    return half4(fragIn.texcoord.x, fragIn.texcoord.y, 0.0, 1.0);
#else
    constexpr sampler pointSampler(filter::nearest);
    half4 texel = dummyTexMap.sample(pointSampler, fragIn.texcoord);
    return fragIn.color * texel;
#endif
}

)foo";
