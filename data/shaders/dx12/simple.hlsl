struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VS_Main(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position;
    result.color = color;

    return result;
}

float4 PS_Main(PSInput input) : SV_TARGET
{
    return input.color;
}