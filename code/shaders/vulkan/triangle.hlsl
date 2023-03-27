

/*
struct VSIn
{
    float3 pos : POSITION;
};
*/

struct VSOut
{
    float4 pos : SV_POSITION;
};

static const float3 triangleVertexData[3] = { float3(0.5, -0.5, 1.0), float3(0.5, 0.5, 1.0), float3(-0.5, 0.5, 1.0) };

VSOut VS_triangle(/* in VSIn input,*/ uint vertexID : SV_VERTEXID)
{
    VSOut output;
    output.pos = float4(triangleVertexData[vertexID], 1.0);
    return output;
}

float4 FS_triangle(in VSOut input) : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}