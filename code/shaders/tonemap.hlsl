#include "shaderinterop_common.h"

//----------------------------------------
cbuffer TonemapParams
{
    uint    inputImageWidth; // convert to uint2
    uint    inputImageHeight; // convert to uint2
    
    uint    inputImagePixelCount;
    float   minLogLuminance;
    float   logLuminanceRange;
    float   oneOverLogLuminanceRange;
    float   exposureKey;
    float   tau;
};
Texture2D<float4> inputImage;

RWTexture2D<float4> outputImage;
RWByteAddressBuffer outputBuffer;
//----------------------------------------

groupshared uint histogramBins[LUMINANCE_HISTOGRAM_BINS_COUNT];

float computeLuminance(float3 color)
{
    const float3 luminanceFactor = float3(0.2126, 0.7152, 0.0722);
    return dot(luminanceFactor, color);
}

[numthreads(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1)]
void csClearOutputLuminanceHistogram(uint groupIndex : SV_GroupIndex)
{
    uint zero = 0;
    outputBuffer.Store(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4, zero);
}

[numthreads(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1)]
void csGenerateHistogram(uint3 id : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    histogramBins[groupIndex] = 0;
    
    GroupMemoryBarrierWithGroupSync();
    
    if(id.x < inputImageWidth && id.y < inputImageHeight)
    {
        float3 hdrColor = inputImage.Load(int3(id.xy, 0)).rgb;
        float luminance = computeLuminance(hdrColor);
        
        uint binIndex = 0;
        if(luminance > 0.001)
        {
            float logLuminance = saturate((log2(luminance) - minLogLuminance) * oneOverLogLuminanceRange);
            binIndex = (uint)(logLuminance * (LUMINANCE_HISTOGRAM_BINS_COUNT - 2) + 1);
        }

        InterlockedAdd(histogramBins[binIndex], 1);
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    outputBuffer.InterlockedAdd(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4, histogramBins[groupIndex]);
}

//----------------------------------------

groupshared float histogram[LUMINANCE_HISTOGRAM_BINS_COUNT];

[numthreads(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1)]
void csComputeAvgLuminance(uint groupIndex : SV_GroupIndex)
{
    float countInBin = (float)outputBuffer.Load(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4);
    histogram[groupIndex] = countInBin * (float)groupIndex;
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for(uint histogramSampleIndex = (LUMINANCE_HISTOGRAM_BINS_COUNT >> 1); histogramSampleIndex > 0; histogramSampleIndex >>= 1)
    {
        if(groupIndex < histogramSampleIndex)
        {
            histogram[groupIndex] += histogram[groupIndex + histogramSampleIndex];
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    if(groupIndex == 0)
    {
        float weightedLogAvg = (histogram[0].x / max((float)inputImagePixelCount - countInBin, 1.0)) - 1.0;
        float weightedAvgLuminance = exp2(((weightedLogAvg / (LUMINANCE_HISTOGRAM_BINS_COUNT - 2)) * logLuminanceRange) + minLogLuminance);
        float luminanceLastFrame = outputBuffer.Load<float>(LUMINANCE_BUFFER_OFFSET_LUMINANCE);
        float adaptedLuminance = luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * (1 - exp(-timeDelta * tau));
        outputBuffer.Store<float>(LUMINANCE_BUFFER_OFFSET_LUMINANCE, adaptedLuminance);
        outputBuffer.Store<float>(LUMINANCE_BUFFER_OFFSET_EXPOSURE, exposureKey / max(adaptedLuminance, 0.0001));
    }
    
    // Enable to clear at the end and remove the need of csClearOutputLuminanceHistogram
    //outputBuffer.Store(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4, 0);
}

//-----------------------------------------------------------------

float3 uncharted2Tonemap(float3 x)
{
    float const A = 0.15;
    float const B = 0.50;
    float const C = 0.10;
    float const D = 0.20;
    float const E = 0.02;
    float const F = 0.30;
    float const W = 11.2;
    
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

[numthreads(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1)]
void csReinhard(uint3 id : SV_DispatchThreadID)
{
    float exposure = outputBuffer.Load<float>(LUMINANCE_BUFFER_OFFSET_EXPOSURE);
    float3 exposedColor = inputImage[id.xy].rgb * exposure;
    float3 tonemappedColor = ACESFilm(exposedColor);
    float3 fc = pow(tonemappedColor, float(1.0 / 2.2));
    outputImage[id.xy] = float4(fc, 1.0);
    return;
}
