#include "shaderinterop_common.h"

Texture2D<float4> inputImage;
RWTexture2D<float4> outputImage;

cbuffer TonemapParams
{
    uint    inputImageWidth;
    uint    inputImageHeight;
    uint    inputImagePixelCount;
    float   minLogLuminance;
    float   logLuminanceRange;
    float   oneOverLogLuminanceRange;
    float   tau;
};

RWByteAddressBuffer luminanceBuffer;

groupshared uint histogramBins[LUMINANCE_HISTOGRAM_BINS_COUNT];

float calculateLuminance(float3 color)
{
    const float3 luminanceFactor = float3(0.2126, 0.7152, 0.0722);
    return dot(luminanceFactor, color);
}

uint calculateBinIndexFromHDRColor(float3 hdrColor)
{
    float lum = calculateLuminance(hdrColor);
    
    float const epsilon = 0.0001;
    if(lum < epsilon)
    {
        return 0;
    }
    
    float logLum = saturate((log2(lum) - minLogLuminance) * oneOverLogLuminanceRange);
    return (uint)(logLum * 254.0 + 1.0);
}

[numthreads(16, 16, 1)]
void csClearOutputLuminanceHistogram(uint groupIndex : SV_GroupIndex)
{
    uint zero = 0;
    luminanceBuffer.Store(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4, zero);
}

[numthreads(16, 16, 1)]
void csGenerateHistogram(uint3 id : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
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
    
    luminanceBuffer.InterlockedAdd(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4, histogramBins[groupIndex]);
}

groupshared float histogramShared[LUMINANCE_HISTOGRAM_BINS_COUNT];

[numthreads(16, 16, 1)]
void csComputeAvgLuminance(uint groupIndex : SV_GroupIndex)
{
    float countInBin = (float)luminanceBuffer.Load(LUMINANCE_BUFFER_OFFSET_HISTOGRAM + groupIndex * 4);
    histogramShared[groupIndex] = countInBin * (float)groupIndex;
    
    GroupMemoryBarrierWithGroupSync();
    
    [unroll]
    for(uint histoSampleIndex = (LUMINANCE_HISTOGRAM_BINS_COUNT >> 1); histoSampleIndex > 0; histoSampleIndex >>= 1)
    {
        if(groupIndex < histoSampleIndex)
        {
            histogramShared[groupIndex] += histogramShared[groupIndex + histoSampleIndex];
        }
        
        GroupMemoryBarrierWithGroupSync();
    }
    
    if(groupIndex == 0)
    {
        float weightedLogAvg = (histogramShared[0].x / max((float)inputImagePixelCount - countInBin, 1.0)) - 1.0;
        float weightedAvgLuminance = exp2(((weightedLogAvg / (LUMINANCE_HISTOGRAM_BINS_COUNT - 2)) * logLuminanceRange) + minLogLuminance);
        float luminanceLastFrame = luminanceBuffer.Load<float>(LUMINANCE_BUFFER_OFFSET_LUMINANCE);
        float adaptedLuminance = luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * (1 - exp(-timeDelta * tau));
        luminanceBuffer.Store(LUMINANCE_BUFFER_OFFSET_LUMINANCE, adaptedLuminance);
    }
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

[numthreads(16, 16, 1)]
void csReinhard(uint3 id : SV_DispatchThreadID)
{
    float avgLum = luminanceBuffer.Load<float>(LUMINANCE_BUFFER_OFFSET_LUMINANCE);
    
    float3 hdrColor = inputImage[id.xy].rgb;
    
    float exposureBias = 1.0;
    float3 curr = uncharted2Tonemap(exposureBias * hdrColor);
    
    float W = avgLum;
    float3 whiteScale = 1.0 / uncharted2Tonemap(W);
    float3 color = curr * whiteScale;
    
    float3 finalColor = color;
    
    outputImage[id.xy] = float4(finalColor, 1.0);
    
    /*
    float avgLum = luminanceBuffer.Load(HISTOGRAM_BIN_COUNT);
    
    float4 colorHdr = inputImage[id.xy];
    float3 colorYxy = convertRGB2Yxy(colorHdr.rgb);
    
    float ld = colorYxy.x / (9.6 * avgLum);
    
    colorYxy.x = reinhard(ld);
    
    float3 tonemappedColor = convertYxy2RGB(colorYxy);
    	
    outputImage[id.xy] = float4(tonemappedColor.rgb, 1.0);
     */
}
