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

#include "shaders/metal/imm_shader.inl"

#if 1
static GfxCtx::Mtl* mtl()
{
    return &g_GfxCtx->mtl;
}
#else
#define mtl() (&g_GfxCtx->mtl)
#endif

static const rgU32 kBindlessTextureSetBinding = 7;
static const rgU32 kFrameParamsSetBinding = 6;

static id<MTLDevice> getMTLDevice()
{
    return (__bridge id<MTLDevice>)(mtl()->device);
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
    
    if(usage == GfxTextureUsage_ShaderWrite)
    {
        result |= MTLTextureUsageShaderWrite;
    }
    
    if(usage == GfxTextureUsage_RenderTarget)
    {
        result |= MTLTextureUsageRenderTarget;
    }
    
    return result;
}

MTLResourceUsage toMTLResourceOptions(GfxTextureUsage usage)
{
    MTLResourceOptions options = MTLStorageModeManaged;
    
    if(usage == GfxTextureUsage_ShaderWrite)
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

id<MTLTexture> getMTLTexture(HGfxTexture2D handle)
{
     return (__bridge id<MTLTexture>)(gfxTexture2DPtr(handle)->mtlTexture);
}

id<MTLBuffer> getActiveMTLBuffer(HGfxBuffer handle)
{
    GfxBuffer* buffer = gfxBufferPtr(handle);
    return (__bridge id<MTLBuffer>)(buffer->mtlBuffers[buffer->activeIdx]);
}

MTLClearColor toMTLClearColor(rgFloat4* color)
{
    return MTLClearColorMake(color->r, color->g, color->b, color->a);
}

id<MTLRenderPipelineState> toMTLRenderPipelineState(GfxGraphicsPSO* pso)
{
    return (__bridge id<MTLRenderPipelineState>)(pso->mtlPSO);
}

id<MTLRenderPipelineState> toMTLRenderPipelineState(HGfxGraphicsPSO handle)
{
    return toMTLRenderPipelineState(gfxGraphicsPSOPtr(handle));
}

id<MTLRenderCommandEncoder> mtlRenderEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)mtl()->renderEncoder;
}

id<MTLCommandBuffer> mtlCommandBuffer()
{
    return (__bridge id<MTLCommandBuffer>)mtl()->commandBuffer;
}

struct SimpleVertexFormat1
{
    simd::float3 position;
    simd::float2 texcoord;
    simd::float4 color;
};

rgInt gfxInit()
{
    GfxCtx* ctx = gfxCtx();
    rgAssert(ctx->mainWindow);
    ctx->mtl.view = (NS::View*)SDL_Metal_CreateView(ctx->mainWindow);
    ctx->mtl.layer = SDL_Metal_GetLayer(ctx->mtl.view);
    ctx->mtl.device = MTL::CreateSystemDefaultDevice();
    ctx->mtl.commandQueue = ctx->mtl.device->newCommandQueue();

    CAMetalLayer* mtlLayer = (CAMetalLayer*)ctx->mtl.layer;
    mtlLayer.device = (__bridge id<MTLDevice>)(ctx->mtl.device);
    mtlLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtlLayer.maximumDrawableCount = RG_MAX_FRAMES_IN_FLIGHT;
    mtlLayer.framebufferOnly = false;
    //

    mtl()->framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);

    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // Make handle value start from 1. Default 0 should be uninitialized handle
    
    HGfxTexture2D t2dptr = gfxNewTexture2D(rg::loadTexture("T.tga"), GfxTextureUsage_ShaderRead);
    
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ctx->renderTarget[i] = gfxNewTexture2D(nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, GfxTextureUsage_RenderTarget, "renderTarget0");
        
        ctx->depthStencilBuffer[i] = gfxNewTexture2D(nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D16_UNORM, GfxTextureUsage_RenderTarget, "depthStencilBuffer");
    }
    
    MTLArgumentDescriptor* argDesc = [MTLArgumentDescriptor argumentDescriptor];
    argDesc.index = 0;
    argDesc.dataType = MTLDataTypeTexture;
    argDesc.arrayLength = 99999;
    argDesc.textureType = MTLTextureType2D;
    
    MTLArgumentDescriptor* argDesc2 = [MTLArgumentDescriptor argumentDescriptor];
    argDesc2.index = 90001;
    argDesc2.dataType = MTLDataTypePointer;
    
    gfxCtx()->mtl.largeArrayTex2DArgEncoder = (__bridge MTL::ArgumentEncoder*)[getMTLDevice() newArgumentEncoderWithArguments: @[argDesc, argDesc2]];
    gfxCtx()->mtl.largeArrayTex2DArgBuffer = gfxNewBuffer(nullptr, mtl()->largeArrayTex2DArgEncoder->encodedLength(), GfxResourceUsage_Dynamic);

    return 0;
}

rgInt gfxDraw()
{
    // TODO: move this from here..
    dispatch_semaphore_wait(mtl()->framesInFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    // --- Autorelease pool BEGIN
    GfxCtx* ctx = gfxCtx();
    CAMetalLayer* metalLayer = (CAMetalLayer*)mtl()->layer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    
    rgAssert(metalDrawable != nullptr);
    if(metalDrawable != nullptr)
    {
        mtl()->commandBuffer = (__bridge id<MTLCommandBuffer>)mtl()->commandQueue->commandBuffer();
        
        // bind all textures
        {
            GfxBuffer* largeArrayTex2DArgBuffer = gfxBufferPtr(mtl()->largeArrayTex2DArgBuffer);
            rgInt argBufferActiveIndex = largeArrayTex2DArgBuffer->activeIdx;
            MTL::Buffer* argBuffer = largeArrayTex2DArgBuffer->mtlBuffers[argBufferActiveIndex];
            
            mtl()->largeArrayTex2DArgEncoder->setArgumentBuffer(argBuffer, 0);
            
            rgSize largeArrayTex2DIndex = 0;
            for(GfxTexture2D* tex2D : gfxCtx()->texture2dManager)
            {
                if(tex2D != nullptr)
                {
                    mtl()->largeArrayTex2DArgEncoder->setTexture(tex2D->mtlTexture, largeArrayTex2DIndex);
                }
                ++largeArrayTex2DIndex;
            }
            
            argBuffer->didModifyRange(NS::Range(0, argBuffer->length()));
        }
        
        gfxGetRenderCmdList()->draw();
        gfxGetRenderCmdList()->afterDraw();

        [mtlRenderEncoder() endEncoding];
        
        // blit renderTarget0 to MTLDrawable
        id<MTLBlitCommandEncoder> blitEncoder = [mtlCommandBuffer() blitCommandEncoder];
        
        id<MTLTexture> srcTexture = getMTLTexture(gfxCtx()->renderTarget[g_FrameIndex]);
        id<MTLTexture> dstTexture = metalDrawable.texture;
        [blitEncoder copyFromTexture:srcTexture toTexture:dstTexture];
        [blitEncoder endEncoding];
        
        [mtlCommandBuffer() presentDrawable:metalDrawable];
        
        __block dispatch_semaphore_t blockSemaphore = mtl()->framesInFlightSemaphore;
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

void gfxDestroy()
{
    
}

GfxBuffer* creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    GfxBuffer* bufferPtr = rgNew(GfxBuffer);
    bufferPtr->usageMode = usage;
    bufferPtr->size = size;
    bufferPtr->activeIdx = 0;
    
    if(usage == GfxResourceUsage_Static)
    {
        rgAssert(data != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if(data != NULL)
        {
            bufferPtr->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithBytes:data length:(NSUInteger)size options:options];
        }
        else
        {
            bufferPtr->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        
        if(usage == GfxResourceUsage_Static)
        {
            break;
        }
    }
    
    return bufferPtr;
}

void updaterGfxBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset)
{
    rgAssert(buffer);
    
    if(buffer->usageMode != GfxResourceUsage_Static)
    {
        if(++buffer->activeIdx >= RG_MAX_FRAMES_IN_FLIGHT)
        {
            buffer->activeIdx = 0;
        }
        
        memcpy((rgU8*)buffer->mtlBuffers[buffer->activeIdx]->contents() + offset, data, size);
        buffer->mtlBuffers[buffer->activeIdx]->didModifyRange(NS::Range(offset, size));
    }
    else
    {
        rgAssert(!"Unimplemented or incorrect usages mode");
    }
}

void deleterGfxBuffer(GfxBuffer* buffer)
{
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        buffer->mtlBuffers[i]->release();
        
        if(buffer->usageMode == GfxResourceUsage_Static)
        {
            break;
        }
    }
    
    rgDelete(buffer);
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

GfxGraphicsPSO* creatorGfxGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* graphicsPSO = rgNew(GfxGraphicsPSO);
    
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
        graphicsPSO->mtlPSO = [getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo reflection:&reflectionInfo error:&err];
        
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newRenderPipelineState error");
        }
        
        [psoDesc release];
        [fs release];
        [vs release];
        [shaderLib release];

        // copy render state info
        graphicsPSO->renderState = *renderStateDesc;
    }
    
    return graphicsPSO;
}

void deleterGfxGraphicsPSO(GfxGraphicsPSO* pso)
{
    [toMTLRenderPipelineState(pso) release];
    rgDelete(pso);
}

GfxTexture2D* creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
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

    // create refobj
    GfxTexture2D* tex2dPtr = rgNew(GfxTexture2D);
    tex2dPtr->width = width;
    tex2dPtr->height = height;
    tex2dPtr->pixelFormat = format;
    tex2dPtr->mtlTexture = (__bridge MTL::Texture*)mtlTexture;
    name != nullptr ? strcpy(tex2dPtr->name, name) : strcpy(tex2dPtr->name, "[NoName]");
    
    return tex2dPtr;
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{
    t2d->mtlTexture->release();
    rgDelete(t2d);
}

void gfxHandleRenderCmd_SetViewport(void const* cmd)
{
    RenderCmd_SetViewport* setViewport = (RenderCmd_SetViewport*)cmd;
    
    MTLViewport vp;
    vp.originX = setViewport->viewport.x;
    vp.originY = setViewport->viewport.y;
    vp.width   = setViewport->viewport.z;
    vp.height  = setViewport->viewport.w;
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    
    [mtlRenderEncoder() setViewport:vp];
}

void gfxHandleRenderCmd_SetRenderPass(void const* cmd)
{
    RenderCmd_SetRenderPass* setRenderPass = (RenderCmd_SetRenderPass*)cmd;
    GfxRenderPass* renderPass = &setRenderPass->renderPass;
    
    // create RenderCommandEncoder
    MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

    for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    {
        if(renderPass->colorAttachments[c].texture == kUninitializedHandle)
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

    mtl()->renderEncoder = [mtlCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    
    [renderPassDesc autorelease];
    
    MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDesc.depthWriteEnabled = true;
    id<MTLDepthStencilState> dsState = [getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    [depthStencilDesc release];
    
    [mtlRenderEncoder() setDepthStencilState:dsState];
    
    for(GfxTexture2D* tex2D : gfxCtx()->texture2dManager)
    {
        if(tex2D != nullptr)
        {
            [mtlRenderEncoder() useResource:(__bridge id<MTLTexture>)tex2D->mtlTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
        }
    }
    
    [mtlRenderEncoder() setFragmentBuffer:getActiveMTLBuffer(mtl()->largeArrayTex2DArgBuffer) offset:0 atIndex:kBindlessTextureSetBinding];
}

void gfxHandleRenderCmd_SetGraphicsPSO(void const* cmd)
{
    RenderCmd_SetGraphicsPSO* setGraphicsPSO = (RenderCmd_SetGraphicsPSO*)cmd;
    [mtlRenderEncoder() setRenderPipelineState:toMTLRenderPipelineState(setGraphicsPSO->pso)];
}

void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd)
{
    RenderCmd_DrawTexturedQuads* rc = (RenderCmd_DrawTexturedQuads*)cmd;
    
    eastl::vector<SimpleVertexFormat> vertices;
    eastl::vector<SimpleInstanceParams> instanceParams;
    
    genTexturedQuadVertices(rc->quads, &vertices, &instanceParams);
    
    if(gfxCtx()->rcTexturedQuadsVB == kUninitializedHandle)
    {
        gfxCtx()->rcTexturedQuadsVB = gfxNewBuffer(nullptr, rgMEGABYTE(16), GfxResourceUsage_Stream);
    }
    
    if(gfxCtx()->rcTexturedQuadsInstParams == kUninitializedHandle)
    {
        gfxCtx()->rcTexturedQuadsInstParams = gfxNewBuffer(nullptr, rgMEGABYTE(4), GfxResourceUsage_Stream);
    }
    
    HGfxBuffer texturesQuadVB = gfxCtx()->rcTexturedQuadsVB;
    HGfxBuffer texturedQuadInstParams = gfxCtx()->rcTexturedQuadsInstParams;
    gfxUpdateBuffer(texturesQuadVB, &vertices.front(), vertices.size() * sizeof(SimpleVertexFormat), 0);
    gfxUpdateBuffer(texturedQuadInstParams, &instanceParams.front(), instanceParams.size() * sizeof(SimpleInstanceParams), 0);
    
    //
    GfxCtx* ctx = gfxCtx();
    
    struct Camera
    {
        float projection[16];
        float view[16];
    } cam;
    
    rgFloat* orthoMatrix = toFloatPtr(ctx->orthographicMatrix);
    rgFloat* viewMatrix = toFloatPtr(ctx->viewMatrix);
    
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

void gfxHandleRenderCmd_DrawTriangles(void const* cmd)
{
    RenderCmd_DrawTriangles* drawTriangles = (RenderCmd_DrawTriangles*)cmd;
        
    rgAssert(drawTriangles->vertexBuffer != kUninitializedHandle);
    
    [mtlRenderEncoder() setVertexBuffer:getActiveMTLBuffer(drawTriangles->vertexBuffer)
                                 offset:drawTriangles->vertexBufferOffset
                                atIndex:0];
    
    if(drawTriangles->indexBuffer != kUninitializedHandle)
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

RG_END_NAMESPACE
#endif
