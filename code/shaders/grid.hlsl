
#include "common.hlsl"

struct VertexShaderOut
{
    float4 position : SV_POSITION;
    float3 nearPoint : NEAR;
    float3 farPoint : FAR;
};

struct FragmentShaderOut
{
    half4 color : SV_TARGET;
    float depth : SV_DEPTH;
};

float3 unprojectClipSpacePoint(float x, float y, float z)
{
    float4 p = mul(mul(cameraInvViewMatrix, cameraInvProjMatrix), float4(x, y, z, 1.0));
    return p.xyz / p.w;
}

VertexShaderOut vsGrid(uint vertexID : SV_VertexID)
{
    const float3 gridPlane[6] = 
    {
        float3(-1, -1, 0), float3(-1, 1, 0), float3(1, 1, 0),
        float3(-1, -1, 0), float3(1, 1, 0), float3(1, -1, 0)
    };

    float3 vertPos = gridPlane[vertexID];

    VertexShaderOut output;
    output.position = float4(vertPos, 1);
    output.nearPoint = unprojectClipSpacePoint(vertPos.x, vertPos.y, 0.0);
    output.farPoint = unprojectClipSpacePoint(vertPos.x, vertPos.y, 1.0);
    return output;
}

float4 grid(float3 fragPos3D, float scale, bool drawAxis) {
    float2 coord = fragPos3D.xz * scale;
    float2 derivative = fwidth(coord);
    float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    float l = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(l, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}

float computeDepth(float3 position)
{
    float4 clipSpacePos = mul(cameraProjMatrix, mul(cameraViewMatrix, float4(position, 1)));
    return (clipSpacePos.z / clipSpacePos.w);
}

// float computeLinearDepth(float clipSpaceDepth)
// {
//     // convert 0 -> 1 to -1.0 to +1.0
//     float d = clipSpaceDepth;// * 2.0 - 1.0;
//     float linearDepth = (2.0 * cameraNear * cameraFar) / (cameraFar + cameraNear - d * (cameraFar - cameraNear));
//     return linearDepth / cameraFar;
// }

FragmentShaderOut fsGrid(in VertexShaderOut f)
{
    /*common.cameraViewMatrix;
    common.camera.viewMatrix;
    common.time.currentTime;
    
    cameraViewMatrix;
    timeCurrentTime;

    camera.viewMatrix;
    time.currentTime;

    viewMatrix;
    currentTime;*/

    float t = -f.nearPoint.y / (f.farPoint.y - f.nearPoint.y);
    float3 pos = f.nearPoint + t * (f.farPoint - f.nearPoint);

    float depth = computeDepth(pos);
    //float linearDepth = computeLinearDepth(depth);

    float fade = max(0, (0.9 - depth));

    float4 color = grid(pos, 1, true) * half(t > 0);
    color.a *= fade;

    FragmentShaderOut output;
    output.color = color;
    output.depth = depth;
    return output;
}