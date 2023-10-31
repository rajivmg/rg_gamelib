// define hlsl to cpp types here
#if __cplusplus
typedef uint32_t uint;
#endif

// common cpp and hlsl code here
static uint const LUMINANCE_BLOCK_SIZE = 16;
static uint const LUMINANCE_HISTOGRAM_BINS_COUNT = LUMINANCE_BLOCK_SIZE * LUMINANCE_BLOCK_SIZE;
static uint const LUMINANCE_BUFFER_OFFSET_EXPOSURE = 0;
static uint const LUMINANCE_BUFFER_OFFSET_LUMINANCE = LUMINANCE_BUFFER_OFFSET_EXPOSURE + 4;
static uint const LUMINANCE_BUFFER_OFFSET_HISTOGRAM = LUMINANCE_BUFFER_OFFSET_LUMINANCE + 4;

static uint const kBindlessTexture2DBindSpace = 7;
#define BINDLESS_TEXTURE2D_SPACE space7

// hlsl only code here
#ifndef __cplusplus

#define Bindless_Texture2D(textureResource) Texture2D<float4> textureResource[] : register(t0, BINDLESS_TEXTURE2D_SPACE);

// common resources and params
cbuffer commonParams// : register(b0, space0)
{
    float3x3 cameraBasisMatrix;
    float4x4 cameraViewMatrix;
    float4x4 cameraProjMatrix;
    float4x4 cameraViewProjMatrix;
    float4x4 cameraInvViewMatrix;
    float4x4 cameraInvProjMatrix;
    float4x4 cameraViewRotOnlyMatrix;
    float    cameraNear;
    float    cameraFar;
    float    timeDelta;
    float    timeGame;
};


// ---------------------------------
struct VS_OUT
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD;
    
#if HAS_VERTEX_NORMAL
    float3 normal   : NORMAL;
#endif
    
    nointerpolation uint instanceID : INSTANCE;
};

VS_OUT vsFullscreenPassthrough(uint vertexID : SV_VERTEXID)
{
    //const float4 vertices[] = { float4(-1.0, -1.0, 0, 1.0), float4(3.0, -1.0, 0, 1.0), float4(-1.0, 3.0, 0, 1.0) };
    const float4 vertices[] = { float4(-1.0, 1.0, 0, 1.0), float4(-1.0, -3.0, 0, 1.0), float4(3.0, 1.0, 0, 1.0) };

    float4 position = vertices[vertexID];
    float2 texcoord = float2(0.5, -0.5) * position.xy + float2(0.5, 0.5);

    VS_OUT output;
    output.position = position;
    output.texcoord = texcoord;
#if HAS_VERTEX_NORMAL
    output.normal = float3(0, 0, -1.0);
#endif
    return output;
}

#endif
