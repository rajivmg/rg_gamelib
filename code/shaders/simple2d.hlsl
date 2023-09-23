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

VertexOut vsSimple2dTest(in Vertex2D v, uint vertexID : SV_VERTEXID, uint instanceID : SV_INSTANCEID)
{
    VertexOut output;
    output.position = float4(v.pos, 1.0);
    output.texcoord = v.texcoord;
    output.color = half4(v.color);
    output.instanceID = vertexID / 6;
    return output;
}

half4 fsSimple2dTest(in VertexOut f) : SV_TARGET
{
    return f.color;
}