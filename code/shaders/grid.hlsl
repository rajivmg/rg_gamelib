
#include "common.hlsl"

struct GridVertexShaderOut
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

GridVertexShaderOut vsGrid(uint vertexID : SV_VertexID)
{
    const float3 gridPlane[6] = 
    {
        float3(-1, -1, 0), float3(-1, 1, 0), float3(1, 1, 0),
        float3(-1, -1, 0), float3(1, 1, 0), float3(1, -1, 0)
    };

    float3 vertPos = gridPlane[vertexID];

    GridVertexShaderOut output;
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
    float4 color = float4(0.8, 0.8, 0.8, 1.0 - min(l, 1.0));
    // z axis
    if(fragPos3D.x > -0.6 * minimumx && fragPos3D.x < 0.6 * minimumx)
    {
        color = float4(0, 0, 1.0, 1.0);
    }
    // x axis
    if(fragPos3D.z > -0.6 * minimumz && fragPos3D.z < 0.6 * minimumz)
    {
        color = float4(1.0, 0, 0, 1.0);
    }
    return color;
}

float computeDepth(float3 position)
{
    float4 clipSpacePos = mul(cameraProjMatrix, mul(cameraViewMatrix, float4(position, 1.0)));
    return (clipSpacePos.z / clipSpacePos.w);
}

float computeLinearDepth(float clipSpaceDepth)
{
    // float d = clipSpaceDepth * 2.0 - 1.0;
    // float4 ndcCoords = float4(0, 0, d, 1.0f);
    // float4 viewCoords = mul(cameraInvProjMatrix, ndcCoords);
    // return (viewCoords.z / viewCoords.w) / cameraFar;

    // float p = -2.0*cameraFar*cameraNear/(clipSpaceDepth * (cameraFar - cameraNear) - (cameraFar + cameraNear));
    // return p / cameraFar;

    float d = clipSpaceDepth * 2.0 - 1.0;
    float linearDepth = (2.0 * cameraNear * cameraFar) / (cameraFar + cameraNear - d * (cameraFar - cameraNear));
    return linearDepth / cameraFar;
}

FragmentShaderOut fsGrid(in GridVertexShaderOut f)
{
    float t = -f.nearPoint.y / (f.farPoint.y - f.nearPoint.y);
    
    float3 pos = f.nearPoint + t * (f.farPoint - f.nearPoint);
    float depth = computeDepth(pos);
    float linearDepth = computeLinearDepth(depth);

    float fade = max(0, (0.5 - linearDepth));

    float4 color = grid(pos, 1, true) * float(t > 0);
    color.a *= fade;

    FragmentShaderOut output;
    output.color = color; // float4(linearDepth, linearDepth, linearDepth, 1.0);//
    output.depth = depth;
    return output;
}
