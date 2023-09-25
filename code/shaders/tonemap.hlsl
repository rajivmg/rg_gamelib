#include "common.hlsl"

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
    float   deltaTime; // TODO: Move to common.hlsl
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
        float adaptedLuminance = luminanceLastFrame + (weightedAvgLuminance - luminanceLastFrame) * (1 - exp(-deltaTime * tau));
        luminanceBuffer.Store(LUMINANCE_BUFFER_OFFSET_LUMINANCE, adaptedLuminance);
    }
}

/// ---

float3 convertRGB2XYZ(float3 _rgb)
{
    // Reference(s):
    // - RGB/XYZ Matrices
    //   https://web.archive.org/web/20191027010220/http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    float3 xyz;
    xyz.x = dot(float3(0.4124564, 0.3575761, 0.1804375), _rgb);
    xyz.y = dot(float3(0.2126729, 0.7151522, 0.0721750), _rgb);
    xyz.z = dot(float3(0.0193339, 0.1191920, 0.9503041), _rgb);
    return xyz;
}

float3 convertXYZ2RGB(float3 _xyz)
{
    float3 rgb;
    rgb.x = dot(float3( 3.2404542, -1.5371385, -0.4985314), _xyz);
    rgb.y = dot(float3(-0.9692660,  1.8760108,  0.0415560), _xyz);
    rgb.z = dot(float3( 0.0556434, -0.2040259,  1.0572252), _xyz);
    return rgb;
}

float3 convertXYZ2Yxy(float3 _xyz)
{
    // Reference(s):
    // - XYZ to xyY
    //   https://web.archive.org/web/20191027010144/http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
    float inv = 1.0/dot(_xyz, float3(1.0, 1.0, 1.0) );
    return float3(_xyz.y, _xyz.x*inv, _xyz.y*inv);
}

float3 convertYxy2XYZ(float3 _Yxy)
{
    // Reference(s):
    // - xyY to XYZ
    //   https://web.archive.org/web/20191027010036/http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
    float3 xyz;
    xyz.x = _Yxy.x*_Yxy.y/_Yxy.z;
    xyz.y = _Yxy.x;
    xyz.z = _Yxy.x*(1.0 - _Yxy.y - _Yxy.z)/_Yxy.z;
    return xyz;
}

float3 convertRGB2Yxy(float3 _rgb)
{
    return convertXYZ2Yxy(convertRGB2XYZ(_rgb) );
}

float3 convertYxy2RGB(float3 _Yxy)
{
    return convertXYZ2RGB(convertYxy2XYZ(_Yxy) );
}

/// ---

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
    
    float exposureBias = 2.0;
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
