#include "common.hlsl"

Texture2D<float4> srcTexture : register(t0, space0);
SamplerState pointSampler : register(s0, space0);

float3 reinhard(float3 c)
{
    return c / (1.0 + c);
}

float4 fsReinhard(VS_OUT f) : SV_TARGET
{
    float4 color = srcTexture.Sample(pointSampler, f.texcoord);
    //color.xyz = reinhard(color.xyz);

    //color.xyz = pow(color.xyz, float3(0.454545, 0.454545, 0.454545));

    return color;
}

//------------------------------------------------------------------------

Texture2D<float4> inputImage;
RWTexture2D<float4> outputImage;

cbuffer TonemapParams
{
    uint    inputImageWidth;
    uint    inputImageHeight;
    float   minLogLuminance;
    float   oneOverLogLuminanceRange;
};

RWByteAddressBuffer outputLuminanceHistogram;

#define HISTOGRAM_BIN_COUNT 256
groupshared uint histogramBins[HISTOGRAM_BIN_COUNT];

float calculateLuminance(float3 color)
{
    const float3 luminanceFactor = float3(0.2126, 0.7152, 0.0722);
    return dot(luminanceFactor, color);
}

uint calculateBinIndexFromHDRColor(float3 hdrColor)
{
    float lum = calculateLuminance(hdrColor);
    
    if(lum < 0.002)
    {
        return 0;
    }
    
    float logLum = saturate((log2(lum) - minLogLuminance) * oneOverLogLuminanceRange);
    return (uint)(logLum * 254.0 + 1.0);
}

[numthreads(16, 16, 1)]
void csClearOutputLuminanceHistogram(uint groupIndex : SV_GroupIndex)
{
    outputLuminanceHistogram.Store(groupIndex * 4, 0);
}

[numthreads(16, 16, 1)]
void csReinhard(uint3 id : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    // TODO: An option to visualize luminance
    float luminance = calculateLuminance(inputImage[id.xy].rgb);
    //outputImage[id.xy] = float4(luminance, luminance, luminance, 1.0);
    outputImage[id.xy] = inputImage[id.xy];
    
    histogramBins[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    if(id.x < inputImageWidth && id.y < inputImageHeight)
    {
        float3 hdrColor = inputImage.Load(int3(id.xy, 0)).rgb;
        uint binIndex = calculateBinIndexFromHDRColor(hdrColor);
        InterlockedAdd(histogramBins[binIndex], 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    outputLuminanceHistogram.InterlockedAdd(groupIndex * 4, histogramBins[groupIndex]);
    
    //outputImage[id.xy] = float4(reinhard(inputImage[id.xy].rgb), inputImage[id.xy].a); //float4(0, 1, 0, 1);
}
