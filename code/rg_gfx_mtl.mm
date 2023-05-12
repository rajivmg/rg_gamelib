#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>
#include <QuartzCore/CAMetalLayer.h>

#include <sstream>

RG_BEGIN_NAMESPACE
RG_GFX_BEGIN_NAMESPACE

#include "shaders/metal/imm_shader.inl"
#include "shaders/metal/histogram_shader.inl"

gfx::Mtl* mtl = nullptr;

static const rgU32 kBindlessTextureSetBinding = 7;
static const rgU32 kFrameParamsSetBinding = 6;

static id<MTLDevice> getMTLDevice()
{
    return (__bridge id<MTLDevice>)(mtl->device);
}

MTLResourceOptions toMTLResourceOptions(GfxResourceUsage usage)
{
    // TODO: recheck the values
    switch(usage)
    {
        case GfxResourceUsage_Static:
        {
            return MTLResourceStorageModeManaged;
        } break;
            
        case GfxResourceUsage_Dynamic:
        case GfxResourceUsage_Stream:
        {
            return MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;
        } break;
            
        default:
            rgAssert(!"Invalid resource usage option");
    }
}

MTLTextureUsage toMTLTextureUsage(GfxTextureUsage usage)
{
    MTLTextureUsage result = MTLTextureUsageUnknown;
    
    if(usage == GfxTextureUsage_ShaderRead)
    {
        result |= MTLTextureUsageShaderRead;
    }
    
    if(usage == GfxTextureUsage_ShaderReadWrite)
    {
        result |= MTLTextureUsageShaderWrite;
    }
    
    if(usage == GfxTextureUsage_RenderTarget)
    {
        result |= MTLTextureUsageRenderTarget;
    }
    
    if(usage == GfxTextureUsage_DepthStencil)
    {
        result |= MTLTextureUsageRenderTarget;
    }
    
    return result;
}

MTLResourceUsage toMTLResourceOptions(GfxTextureUsage usage)
{
    MTLResourceOptions options = MTLStorageModeManaged;
    
    if(usage == GfxTextureUsage_ShaderReadWrite)
    {
        options |= MTLResourceCPUCacheModeWriteCombined;
    }

    return options;
}

MTLLoadAction toMTLLoadAction(GfxLoadAction action)
{
    MTLLoadAction result = MTLLoadActionDontCare;
    
    if(action == GfxLoadAction_Clear)
    {
        result = MTLLoadActionClear;
    }
    else if(action == GfxLoadAction_Load)
    {
        result = MTLLoadActionLoad;
    }
    
    return result;
}

MTLStoreAction toMTLStoreAction(GfxStoreAction action)
{
    MTLStoreAction result = MTLStoreActionDontCare;
    
    if(action == GfxStoreAction_Store)
    {
        result = MTLStoreActionStore;
    }
    
    return result;
};

MTLPixelFormat toMTLPixelFormat(TinyImageFormat fmt)
{
    return (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(fmt);
}

id<MTLTexture> getMTLTexture(GfxTexture2D* ptr)
{
     return (__bridge id<MTLTexture>)(ptr->mtlTexture);
}

id<MTLBuffer> getActiveMTLBuffer(GfxBuffer* ptr)
{
    return (__bridge id<MTLBuffer>)(ptr->mtlBuffers[ptr->activeSlot]);
}

MTLClearColor toMTLClearColor(rgFloat4* color)
{
    return MTLClearColorMake(color->r, color->g, color->b, color->a);
}

id<MTLRenderPipelineState> toMTLRenderPipelineState(GfxGraphicsPSO* pso)
{
    return (__bridge id<MTLRenderPipelineState>)(pso->mtlPSO);
}

id<MTLRenderCommandEncoder> mtlRenderEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)mtl->renderEncoder;
}

id<MTLCommandBuffer> mtlCommandBuffer()
{
    return (__bridge id<MTLCommandBuffer>)mtl->commandBuffer;
}

struct SimpleVertexFormat1
{
    simd::float3 position;
    simd::float2 texcoord;
    simd::float4 color;
};

id<MTLComputePipelineState> histogramComputePipeline;
///GfxTexture2D* histogramTestTexture;

void testComputeAtomicsSetup()
{
    MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
    
    NSError* err;
    id<MTLLibrary> histogramLibrary = [getMTLDevice() newLibraryWithSource:[NSString stringWithUTF8String:g_HistogramShaderSrcCode] options:compileOptions error:&err];
    if(err)
    {
        printf("%s\n", [[err localizedDescription]UTF8String]);
        rgAssert(!"newLibrary error");
    }
    [compileOptions release];
    
    id<MTLFunction> computeHistogram = [histogramLibrary newFunctionWithName:@"computeHistogram_CS"];
    
    histogramComputePipeline = [getMTLDevice() newComputePipelineStateWithFunction:computeHistogram error:&err];
    
    [histogramLibrary release];
    
    gfx::createTexture2D("histogramTest", rg::loadTexture("histogram_test.png"), GfxTextureUsage_ShaderRead);
    gfx::createBuffer("histogramBuffer", nullptr, sizeof(rgUInt)*255*3, GfxResourceUsage_Dynamic);
}

void testComputeAtomicsRun()
{
    id<MTLComputeCommandEncoder> computeEncoder = [mtlCommandBuffer() computeCommandEncoder];
    [computeEncoder setComputePipelineState:histogramComputePipeline];
    [computeEncoder setTexture:getMTLTexture(gfx::findTexture2D("histogramTest")) atIndex:0];
    [computeEncoder setBuffer:getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) offset:0 atIndex:0];
    [computeEncoder setBuffer:getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) offset:0 atIndex:1];
    [computeEncoder dispatchThreads:MTLSizeMake(4, 4, 1) threadsPerThreadgroup:MTLSizeMake(4, 4, 1)];
    [computeEncoder endEncoding];
}

rgInt init()
{
    mtl = rgNew(gfx::Mtl);
    
    rgAssert(gfx::mainWindow);
    mtl->view = (NS::View*)SDL_Metal_CreateView(gfx::mainWindow);
    mtl->layer = SDL_Metal_GetLayer(mtl->view);
    mtl->device = MTL::CreateSystemDefaultDevice();
    mtl->commandQueue = mtl->device->newCommandQueue();

    CAMetalLayer* mtlLayer = (CAMetalLayer*)mtl->layer;
    mtlLayer.device = (__bridge id<MTLDevice>)(mtl->device);
    mtlLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtlLayer.maximumDrawableCount = RG_MAX_FRAMES_IN_FLIGHT;
    mtlLayer.framebufferOnly = false;
    //

    mtl->framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);

    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // Make handle value start from 1. Default 0 should be uninitialized handle
    
    //GfxTexture2D t2dptr = gfxNewTexture2D(rg::loadTexture("T.tga"), GfxTextureUsage_ShaderRead);
    
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        eastl::string tag = "renderTarget" + i;
        gfx::renderTarget[i] = createTexture2D(tag.c_str(), nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, GfxTextureUsage_RenderTarget);
    }
    gfx::depthStencilBuffer = createTexture2D("depthStencilBuffer", nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D16_UNORM, GfxTextureUsage_DepthStencil);
    
    MTLArgumentDescriptor* argDesc = [MTLArgumentDescriptor argumentDescriptor];
    argDesc.index = 0;
    argDesc.dataType = MTLDataTypeTexture;
    argDesc.arrayLength = 99999;
    argDesc.textureType = MTLTextureType2D;
    
    MTLArgumentDescriptor* argDesc2 = [MTLArgumentDescriptor argumentDescriptor];
    argDesc2.index = 90001;
    argDesc2.dataType = MTLDataTypePointer;
    
    mtl->largeArrayTex2DArgEncoder = (__bridge MTL::ArgumentEncoder*)[getMTLDevice() newArgumentEncoderWithArguments: @[argDesc, argDesc2]];
    mtl->largeArrayTex2DArgBuffer = createBuffer("largeArrayTex2DArgBuffer", nullptr, mtl->largeArrayTex2DArgEncoder->encodedLength(), GfxResourceUsage_Dynamic);

    testComputeAtomicsSetup();
    return 0;
}

rgInt draw()
{
    // TODO: move this from here..
    dispatch_semaphore_wait(mtl->framesInFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    // --- Autorelease pool BEGIN
    CAMetalLayer* metalLayer = (CAMetalLayer*)mtl->layer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    
    rgAssert(metalDrawable != nullptr);
    if(metalDrawable != nullptr)
    {
        mtl->commandBuffer = (__bridge id<MTLCommandBuffer>)mtl->commandQueue->commandBuffer();
        
        // bind all textures
        {
            GfxBuffer* largeArrayTex2DArgBuffer = mtl->largeArrayTex2DArgBuffer;
            rgInt argBufferActiveIndex = largeArrayTex2DArgBuffer->activeSlot;
            MTL::Buffer* argBuffer = largeArrayTex2DArgBuffer->mtlBuffers[argBufferActiveIndex];
            
            mtl->largeArrayTex2DArgEncoder->setArgumentBuffer(argBuffer, 0);
            
            rgSize largeArrayTex2DIndex = 0;
            for(GfxTexture2D* texture2d : *gfx::bindlessManagerTexture2D)
            {
                if(texture2d != nullptr)
                {
                    mtl->largeArrayTex2DArgEncoder->setTexture(texture2d->mtlTexture, largeArrayTex2DIndex);
                }
                ++largeArrayTex2DIndex;
            }
            
            argBuffer->didModifyRange(NS::Range(0, argBuffer->length()));
        }
        
        testComputeAtomicsRun();
        
        getRenderCmdList()->draw();
        getRenderCmdList()->afterDraw();

        [mtlRenderEncoder() endEncoding];
        
        // blit renderTarget0 to MTLDrawable
        id<MTLBlitCommandEncoder> blitEncoder = [mtlCommandBuffer() blitCommandEncoder];
        
        id<MTLTexture> srcTexture = getMTLTexture(gfx::renderTarget[g_FrameIndex]);
        id<MTLTexture> dstTexture = metalDrawable.texture;
        [blitEncoder copyFromTexture:srcTexture toTexture:dstTexture];
        [blitEncoder endEncoding];
        
        [mtlCommandBuffer() presentDrawable:metalDrawable];
        
        __block dispatch_semaphore_t blockSemaphore = mtl->framesInFlightSemaphore;
        [mtlCommandBuffer() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
         {
            dispatch_semaphore_signal(blockSemaphore);
        }];
        
        [mtlCommandBuffer() commit];
    }
    else
    {
        rgLogError("Current mtl drawable is null");
        // exit
    }
    // --- Autorelease pool END
    arp->release();
    
    return 0;
}

void destroy()
{
    
}

void onSizeChanged()
{
    
}

void creatorGfxBuffer(char const* tag, void* buf, rgU32 size, GfxResourceUsage usage, GfxBuffer* obj)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    if(usage == GfxResourceUsage_Static)
    {
        rgAssert(buf != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if(buf != NULL)
        {
            obj->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithBytes:buf length:(NSUInteger)size options:options];
        }
        else
        {
            obj->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        
        if(usage == GfxResourceUsage_Static)
        {
            break;
        }
    }
}

void updaterGfxBuffer(void* data, rgUInt size, rgUInt offset, GfxBuffer* buffer)
{
    rgAssert(buffer);
    
    if(buffer->usage != GfxResourceUsage_Static)
    {
        if(++buffer->activeSlot >= RG_MAX_FRAMES_IN_FLIGHT)
        {
            buffer->activeSlot = 0;
        }
        
        memcpy((rgU8*)buffer->mtlBuffers[buffer->activeSlot]->contents() + offset, data, size);
        buffer->mtlBuffers[buffer->activeSlot]->didModifyRange(NS::Range(offset, size));
    }
    else
    {
        rgAssert(!"Unimplemented or incorrect usages mode");
    }
}

void destroyerGfxBuffer(GfxBuffer* obj)
{
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        obj->mtlBuffers[i]->release();
        
        if(obj->usage == GfxResourceUsage_Static)
        {
            break;
        }
    }
}

NSDictionary* macrosToNSDictionary(char const* macros)
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
    
    return (NSDictionary*)(macroDictionary);
}

void creatorGfxGraphicsPSO(char const* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
{
    @autoreleasepool
    {
        // --- compile shader
        NSDictionary* shaderMacros = macrosToNSDictionary(shaderDesc->macros);
        
        MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
        [compileOptions setPreprocessorMacros:shaderMacros];
        [shaderMacros autorelease];
        
        NSError* err;
        id<MTLLibrary> shaderLib = [getMTLDevice() newLibraryWithSource:[NSString stringWithUTF8String:shaderDesc->shaderSrcCode] options:compileOptions error:&err];
        if(err)
        {
            printf("%s\n", [[err localizedDescription]UTF8String]);
            rgAssert(!"newLibrary error");
        }
        [compileOptions release];
    
        // --- create PSO
        id<MTLFunction> vs, fs;
        if(shaderDesc->vsEntryPoint)
        {
            vs = [shaderLib newFunctionWithName:[NSString stringWithUTF8String: shaderDesc->vsEntryPoint]];
        }
        if(shaderDesc->fsEntryPoint)
        {
            fs = [shaderLib newFunctionWithName:[NSString stringWithUTF8String: shaderDesc->fsEntryPoint]];
        }
    
        MTLRenderPipelineDescriptor* psoDesc = [[MTLRenderPipelineDescriptor alloc] init];
        [psoDesc setVertexFunction:vs];
        [psoDesc setFragmentFunction:fs];
        
        rgAssert(renderStateDesc != nullptr);
        for(rgInt i = 0; i < kMaxColorAttachments; ++i)
        {
            TinyImageFormat colorAttFormat = renderStateDesc->colorAttachments[i].pixelFormat;
            if(colorAttFormat != TinyImageFormat_UNDEFINED)
            {
                rgBool blendingEnabled = renderStateDesc->colorAttachments[i].blendingEnabled;
                psoDesc.colorAttachments[i].pixelFormat = toMTLPixelFormat(colorAttFormat);
                psoDesc.colorAttachments[i].blendingEnabled = NO;
                if(blendingEnabled)
                {
                    psoDesc.colorAttachments[i].blendingEnabled = YES;
                    psoDesc.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
                    psoDesc.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                    psoDesc.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                    
                    psoDesc.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
                    psoDesc.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
                    psoDesc.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorZero;
                }
            }
        }
        
        if(renderStateDesc->depthStencilAttachmentFormat != TinyImageFormat_UNDEFINED)
        {
            psoDesc.depthAttachmentPixelFormat = toMTLPixelFormat(renderStateDesc->depthStencilAttachmentFormat);
        }
        
        // TODO TODO TODO TODO REMOVE
        // TODO TODO TODO TODO REMOVE
        MTLAutoreleasedRenderPipelineReflection reflectionInfo;
        // TODO TODO TODO TODO REMOVE
        // TODO TODO TODO TODO REMOVE

        //[getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc error:&err];
        obj->mtlPSO = [getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo reflection:&reflectionInfo error:&err];
        
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newRenderPipelineState error");
        }
        
        [psoDesc release];
        [fs release];
        [vs release];
        [shaderLib release];
    }
}

void deleterGfxGraphicsPSO(GfxGraphicsPSO* pso)
{
    [toMTLRenderPipelineState(pso) release];
    rgDelete(pso);
}

void creatorGfxTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, GfxTexture2D* obj)
{
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = MTLTextureType2D;
    texDesc.storageMode = MTLStorageModeManaged;
    texDesc.usage = toMTLTextureUsage(usage);
    texDesc.resourceOptions = toMTLResourceOptions(usage);
    
    id<MTLTexture> mtlTexture = [getMTLDevice() newTextureWithDescriptor:texDesc];
    [texDesc release];
    
    // copy the texture data
    if(buf != NULL)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:buf bytesPerRow:width * TinyImageFormat_ChannelCount(format)];
    }

    obj->mtlTexture = (__bridge MTL::Texture*)mtlTexture;
}

void destroyerGfxTexture2D(GfxTexture2D* obj)
{
    obj->mtlTexture->release();
}

void creatorGfxRenderTarget(char const* tag, rgU32 width, rgU32 height, TinyImageFormat format, GfxRenderTarget* obj)
{
}

void handleGfxCmd_SetViewport(void const* cmd)
{
    GfxCmd_SetViewport* setViewport = (GfxCmd_SetViewport*)cmd;
    
    MTLViewport vp;
    vp.originX = setViewport->viewport.x;
    vp.originY = setViewport->viewport.y;
    vp.width   = setViewport->viewport.z;
    vp.height  = setViewport->viewport.w;
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    
    [mtlRenderEncoder() setViewport:vp];
}

void handleGfxCmd_SetRenderPass(void const* cmd)
{
    GfxCmd_SetRenderPass* setRenderPass = (GfxCmd_SetRenderPass*)cmd;
    GfxRenderPass* renderPass = &setRenderPass->renderPass;
    
    // create RenderCommandEncoder
    MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

    for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    {
        if(renderPass->colorAttachments[c].texture == NULL)
        {
            continue;
        }
        
        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][c];
        
        colorAttachmentDesc.texture = getMTLTexture(renderPass->colorAttachments[c].texture);
        colorAttachmentDesc.clearColor = toMTLClearColor(&renderPass->colorAttachments[c].clearColor);
        colorAttachmentDesc.loadAction = toMTLLoadAction(renderPass->colorAttachments[c].loadAction);
        colorAttachmentDesc.storeAction = toMTLStoreAction(renderPass->colorAttachments[c].storeAction);
    }
    
    MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
    depthAttachmentDesc.texture  = getMTLTexture(renderPass->depthStencilAttachmentTexture);
    depthAttachmentDesc.clearDepth = renderPass->clearDepth;
    depthAttachmentDesc.loadAction = toMTLLoadAction(renderPass->depthStencilAttachmentLoadAction);
    depthAttachmentDesc.storeAction = toMTLStoreAction(renderPass->depthStencilAttachmentStoreAction);

    mtl->renderEncoder = [mtlCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    
    [renderPassDesc autorelease];
    
    MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDesc.depthWriteEnabled = true;
    id<MTLDepthStencilState> dsState = [getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    [depthStencilDesc release];
    
    [mtlRenderEncoder() setDepthStencilState:dsState];
    
    for(GfxTexture2D* texture2d : *gfx::bindlessManagerTexture2D)
    {
        if(texture2d != nullptr)
        {
            [mtlRenderEncoder() useResource:(__bridge id<MTLTexture>)texture2d->mtlTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
        }
    }
    
    [mtlRenderEncoder() setFragmentBuffer:getActiveMTLBuffer(mtl->largeArrayTex2DArgBuffer) offset:0 atIndex:kBindlessTextureSetBinding];
}

void handleGfxCmd_SetGraphicsPSO(void const* cmd)
{
    GfxCmd_SetGraphicsPSO* setGraphicsPSO = (GfxCmd_SetGraphicsPSO*)cmd;
    [mtlRenderEncoder() setRenderPipelineState:toMTLRenderPipelineState(setGraphicsPSO->pso)];
}

void handleGfxCmd_DrawTexturedQuads(void const* cmd)
{
    GfxCmd_DrawTexturedQuads* rc = (GfxCmd_DrawTexturedQuads*)cmd;
    
    eastl::vector<SimpleVertexFormat> vertices;
    eastl::vector<SimpleInstanceParams> instanceParams;
    
    genTexturedQuadVertices(rc->quads, &vertices, &instanceParams);
    
    if(gfx::rcTexturedQuadsVB == NULL)
    {
        gfx::rcTexturedQuadsVB = createBuffer("texturedQuadsVB", nullptr, rgMEGABYTE(16), GfxResourceUsage_Stream);
    }
    
    if(gfx::rcTexturedQuadsInstParams == NULL)
    {
        gfx::rcTexturedQuadsInstParams = createBuffer("texturedQuadInstParams", nullptr, rgMEGABYTE(4), GfxResourceUsage_Stream);
    }
    
    GfxBuffer* texturesQuadVB = gfx::rcTexturedQuadsVB;
    GfxBuffer* texturedQuadInstParams = gfx::rcTexturedQuadsInstParams;
    updateBuffer("texturedQuadsVB", &vertices.front(), vertices.size() * sizeof(SimpleVertexFormat), 0);
    updateBuffer("texturedQuadInstParams", &instanceParams.front(), instanceParams.size() * sizeof(SimpleInstanceParams), 0);
    
    //
    struct Camera
    {
        float projection[16];
        float view[16];
    } cam;
    
    rgFloat* orthoMatrix = toFloatPtr(gfx::orthographicMatrix);
    rgFloat* viewMatrix = toFloatPtr(gfx::viewMatrix);
    
    for(rgInt i = 0; i < 16; ++i)
    {
        cam.projection[i] = orthoMatrix[i];
        cam.view[i] = viewMatrix[i];
    }
     
    [mtlRenderEncoder() setRenderPipelineState:toMTLRenderPipelineState(rc->pso)];
    [mtlRenderEncoder() setVertexBytes:&cam length:sizeof(Camera) atIndex:0];
    [mtlRenderEncoder() setFragmentBuffer:getActiveMTLBuffer(texturedQuadInstParams) offset:0 atIndex:4];
    [mtlRenderEncoder() setVertexBuffer:getActiveMTLBuffer(texturesQuadVB) offset:0 atIndex:1];
    [mtlRenderEncoder() setCullMode:MTLCullModeNone];
    
    MTLViewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = 720;
    viewport.height = 720;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    [mtlRenderEncoder() setViewport:viewport];
    [mtlRenderEncoder() drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:vertices.size()/6];
}

void handleGfxCmd_DrawTriangles(void const* cmd)
{
    GfxCmd_DrawTriangles* drawTriangles = (GfxCmd_DrawTriangles*)cmd;
        
    rgAssert(drawTriangles->vertexBuffer != NULL);
    
    [mtlRenderEncoder() setVertexBuffer:getActiveMTLBuffer(drawTriangles->vertexBuffer)
                                 offset:drawTriangles->vertexBufferOffset
                                atIndex:0];
    
    if(drawTriangles->indexBuffer != NULL)
    {
        [mtlRenderEncoder() drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                       indexCount:drawTriangles->indexCount
                                        indexType:MTLIndexTypeUInt16
                                      indexBuffer:getActiveMTLBuffer(drawTriangles->indexBuffer)
                                indexBufferOffset:drawTriangles->indexBufferOffset
                                    instanceCount:drawTriangles->instanceCount
                                       baseVertex:drawTriangles->baseVertex
                                     baseInstance:drawTriangles->baseInstance];
    }
    else
    {
        [mtlRenderEncoder() drawPrimitives:MTLPrimitiveTypeTriangle
                               vertexStart:drawTriangles->baseVertex
                               vertexCount:drawTriangles->vertexCount
                             instanceCount:drawTriangles->instanceCount
                              baseInstance:drawTriangles->baseInstance];
    }
}

RG_GFX_END_NAMESPACE
RG_END_NAMESPACE
#endif
