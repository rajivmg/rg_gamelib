
// common functions and types

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

