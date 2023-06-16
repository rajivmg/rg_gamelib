static char const* g_Simple2DShaderSrcCode = R"foo(
#include <metal_stdlib>
using namespace metal;

struct SimpleVertexIn
{
    float3 pos [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
    float4 color [[attribute(2)]];
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

struct CommonFrameParams
{
    float4x4 cameraProjection2d;
    float4x4 cameraView2d;
};

struct DescSpace0
{
    constant CommonFrameParams* commonFrameParams [[id(0)]];
};

struct BindlessResources
{
    array<texture2d<float>, 99999> textures2d [[id(0)]];
};

VertexOut vertex simple2d_VS(constant DescSpace0& descSpace0 [[buffer(0)]],
                             constant SimpleInstanceParams* instanceParams [[buffer(2)]],
                             SimpleVertexIn in [[stage_in]],
                             uint vertexId [[vertex_id]],
                             uint instanceId [[instance_id]])
{
    VertexOut out;
    out.position = descSpace0.commonFrameParams->cameraProjection2d * descSpace0.commonFrameParams->cameraView2d * float4(in.pos, 1.0);
    out.texcoord = in.texcoord;
    out.color  = half4(in.color);
    out.instanceId = vertexId / 6;
    return out;
}

fragment half4 simple2d_FS(VertexOut fragIn [[stage_in]],
                           constant SimpleInstanceParams* instanceParams [[buffer(4)]],
                           device BindlessResources& bindlessResources [[buffer(7)]])
{
    constexpr sampler pointSampler(filter::nearest);
    texture2d<float> myTexture = bindlessResources.textures2d[instanceParams[fragIn.instanceId].texID];
    float4 color = float4(1.0, 0.0, 1.0, 1.0);
    if(!is_null_texture(myTexture))
    {
        color = myTexture.sample(pointSampler, fragIn.texcoord);
    }
    return half4(color);
}

)foo";
