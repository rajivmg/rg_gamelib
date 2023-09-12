
// ---------------------------------
// common resources and params
cbuffer commonParams : register(b0, space0)
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
