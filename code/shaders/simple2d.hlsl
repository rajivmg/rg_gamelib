#define MAX_INSTANCES 1024

struct Vertex2D
{
    float3 pos  : POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};
struct VertexOut
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    half4 color : COLOR;
    nointerpolation uint instanceID : INSTANCE;
};

struct SimpleInstanceParams
{
    uint4 texID[MAX_INSTANCES];
};

struct Camera
{
    float4x4 projection2d;
    float4x4 view2d;
};

ConstantBuffer<Camera> camera : register(b0, space0);
ConstantBuffer<SimpleInstanceParams> instanceParams : register(b1, space0);
Texture2D<float4> bindlessTexture2D[] : register(t0, space7);

SamplerState simpleSampler : register(s0, space0);

VertexOut vsSimple2d(in Vertex2D v, uint vertexID : SV_VERTEXID, uint instanceID : SV_INSTANCEID)
{
    VertexOut output;
    output.position = mul(camera.projection2d, mul(camera.view2d, float4(v.pos, 1.0)));
    output.texcoord = v.texcoord;
    output.color  = half4(v.color);
    output.instanceID = vertexID / 6;
    return output;
}

half4 fsSimple2d(in VertexOut f) : SV_TARGET
{
    //return f.color;
    uint texIndex = instanceParams.texID[f.instanceID].x;
    half4 color = bindlessTexture2D[texIndex].Sample(simpleSampler, f.texcoord);
    return half4(color);
}

/*
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

VertexOut vertex simple2d_VS(constant Camera& camera [[buffer(0)]],
                             constant Vertex2D* vertexBuffer [[buffer(1)]],
                             constant SimpleInstanceParams* instanceParams [[buffer(2)]],
                             uint vertexId [[vertex_id]],
                             uint instanceId [[instance_id]])
{
    VertexOut out;
    constant Vertex2D* v = &vertexBuffer[instanceId * 6 + vertexId];
    out.position = camera.projection * camera.view * float4(v->pos, 1.0, 1.0);
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
*/
