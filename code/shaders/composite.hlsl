#include "shaderinterop_common.h"

cbuffer CompositeParams
{
    uint2    inputImageDim;
    //uint2    _pad;
};

Texture2D<float4> inputImage;
RWTexture2D<float4> outputImage;

[numthreads(16, 16, 1)]
void csComposite(uint3 id : SV_DispatchThreadID)
{
    if(id.x < inputImageDim.x && id.y < inputImageDim.y)
    {
        float4 src = inputImage[id.xy];
        float4 dst = outputImage[id.xy];
        
        float4 blendedColor;
        blendedColor.a = src.a * 1.0 + dst.a * 0;
        blendedColor.rgb = src.rgb * src.a + dst.rgb * dst.a * (1.0 - src.a);
        
        //if(inputImage[id.xy].a != 0)
        {
            //outputImage[id.xy] = inputImage[id.xy];
            outputImage[id.xy] = blendedColor;
        }
    }
}
