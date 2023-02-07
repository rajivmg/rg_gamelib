#include "metal_utils.h"

#include <QuartzCore/CAMetalLayer.h>
#include <CoreVideo/CVDisplayLink.h>
#include <sstream>

namespace metalutils
{
void initMetal(rg::GfxCtx* ctx)
{
    CAMetalLayer* mtlLayer = (CAMetalLayer*)ctx->mtl.layer;
    mtlLayer.device = (__bridge id<MTLDevice>)(ctx->mtl.device);
    mtlLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
}

CA::MetalDrawable* nextDrawable(rg::GfxCtx* ctx)
{
    CAMetalLayer* mtlLayer = (CAMetalLayer*)ctx->mtl.layer;
    return (__bridge CA::MetalDrawable*)[mtlLayer nextDrawable];
}

static CVReturn gfxDrawCVDisplayLinkHandler(CVDisplayLinkRef displayLink,
                           const CVTimeStamp* inNow,
                           const CVTimeStamp* inOutputTime,
                           CVOptionFlags flagsIn,
                           CVOptionFlags* flagsOut)
{
    rg::gfxDraw();
    return 0;
}

NS::Dictionary* getPreprocessorMacrosDict(char const* macros)
{
    // TODO: Remember to free macroDictionary
    NSMutableDictionary *macroDictionary = [[NSMutableDictionary alloc] init];
    
    char const* cursor = macros;
    std::stringstream macrostring;
    while(1)
    {
        if(*cursor == ' ' || *cursor == ',' || *cursor == '\0')
        {
            if(macrostring.str().length() > 0)
            {
                [macroDictionary setValue:@"1" forKey:[NSString stringWithUTF8String:macrostring.str().c_str()]];
                macrostring.str(std::string());
            }
            if(*cursor == '\0')
            {
                break;
            }
            //ppDict->dictionary(NS::UInteger(1), NS::String::string(macro.str().c_str(), NS::UTF8StringEncoding));
        }
        else
        {
            macrostring.put(*cursor);
        }
        ++cursor;
    }
    
    //NSLog(@"PreprocessorDict Keys: %@\n", [macroDictionary allKeys]);
    //NSLog(@"PreprocessorDict Vals: %@\n", [macroDictionary allValues]);
    NSLog(@"PreprocessorDict KeyVals: %@\n", macroDictionary);
    
    return (__bridge NS::Dictionary*)(macroDictionary);
}

/*
void SetupCVDisplayLinkBasedRenderCallback()
{
    CVDisplayLinkRef displayLink;
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputHandler(displayLink, &gfxDrawCVDisplayLinkHandler);
}
*/
}
