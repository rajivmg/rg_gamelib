static char const* g_HistogramShaderSrcCode = R"foo(
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

#define USE_ATOMICS

uint4 float01ToUInt0255(float4 v)
{
    return uint4(v.r * 255, v.g * 255, v.b * 255, v.a * 255);
}

kernel void computeHistogram_CS(texture2d<float, access::read> inputTexture [[texture(0)]],
                                device uint* outHistogram [[buffer(0)]],
#ifdef USE_ATOMICS
                                device atomic_uint* outHistogramAtomic [[buffer(1)]],
#endif
                                uint2 tidXY [[thread_position_in_grid]])
{
    uint2 textureSize(inputTexture.get_width(), inputTexture.get_height());

    // clear out buffer
    for(int i = 0; i < (255 * 3); ++i)
    {
        outHistogram[i] = 0;
    }
    
    uint4 pixelColor = float01ToUInt0255(inputTexture.read(tidXY));

#ifdef USE_ATOMICS
    atomic_fetch_add_explicit((device atomic_uint*)&outHistogramAtomic[pixelColor.r + (256 * 0)], 1, memory_order_relaxed);
    atomic_fetch_add_explicit((device atomic_uint*)&outHistogramAtomic[pixelColor.g + (256 * 1)], 1, memory_order_relaxed);
    atomic_fetch_add_explicit((device atomic_uint*)&outHistogramAtomic[pixelColor.b + (256 * 2)], 1, memory_order_relaxed);
#else
    if(all(tidXY < textureSize))
    {
        uint histR = outHistogram[pixelColor.r + (256 * 0)];
        outHistogram[pixelColor.r + (256 * 0)] = histR + 1;

        uint histG = outHistogram[pixelColor.g + (256 * 1)];
        outHistogram[pixelColor.g + (256 * 1)] = histG + 1;

        uint histB = outHistogram[pixelColor.b + (256 * 2)];
        outHistogram[pixelColor.b + (256 * 2)] = histB + 1;
    }
#endif
}

)foo";
