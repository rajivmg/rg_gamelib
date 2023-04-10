#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"
#include "metal_utils.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>
#include <QuartzCore/CAMetalLayer.h>

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
        ctx->renderTarget0[i] = gfxNewTexture2D(nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, GfxTextureUsage_RenderTarget, "renderTarget0");
        
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
            for(GfxTexture2DRef tex2D : gfxCtx()->texture2dManager)
            {
                mtl()->largeArrayTex2DArgEncoder->setTexture(tex2D->mtlTexture, largeArrayTex2DIndex);
                ++largeArrayTex2DIndex;
            }
            
            argBuffer->didModifyRange(NS::Range(0, argBuffer->length()));
        }
        
        gfxGetRenderCmdList()->draw();
        gfxGetRenderCmdList()->afterDraw();

        [mtlRenderEncoder() endEncoding];
        
        // blit renderTarget0 to MTLDrawable
        id<MTLBlitCommandEncoder> blitEncoder = [mtlCommandBuffer() blitCommandEncoder];
        
        id<MTLTexture> srcTexture = getMTLTexture(gfxCtx()->renderTarget0[g_FrameIndex]);
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

GfxBufferRef creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    GfxBufferRef bufferRef = GfxBufferRef(rgNew(GfxBuffer), deleterGfxBuffer);
    bufferRef->usageMode = usage;
    bufferRef->size = size;
    bufferRef->activeIdx = 0;
    
    if(usage == GfxResourceUsage_Static)
    {
        rgAssert(data != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if(data != NULL)
        {
            bufferRef->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithBytes:data length:(NSUInteger)size options:options];
        }
        else
        {
            bufferRef->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        
        if(usage == GfxResourceUsage_Static)
        {
            break;
        }
    }
    
    return bufferRef;
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

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* graphicsPSO = rgNew(GfxGraphicsPSO);
    
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    
    // --- compile shader
    NS::Dictionary* shaderMacros = metalutils::getPreprocessorMacrosDict(shaderDesc->macros);
    MTL::CompileOptions* compileOptions = MTL::CompileOptions::alloc()->init();
    compileOptions->setPreprocessorMacros(shaderMacros);
    shaderMacros->autorelease();
    
    NS::Error* err = nullptr;
    
    MTL::Library* shaderLib = mtl()->device->newLibrary(NS::String::string(shaderDesc->shaderSrcCode, NS::UTF8StringEncoding), compileOptions, &err);
    
    if(err)
    {
        printf("%s\n", err->localizedDescription()->utf8String());
        rgAssert(!"newLibrary error");
    }
    
    compileOptions->release();
    
    MTL::Function* vs = nullptr;
    MTL::Function* fs = nullptr;
    
    if(shaderDesc->vsEntryPoint)
    {
        vs = shaderLib->newFunction(NS::String::string(shaderDesc->vsEntryPoint, NS::UTF8StringEncoding));
    }
    
    if(shaderDesc->fsEntryPoint)
    {
        fs = shaderLib->newFunction(NS::String::string(shaderDesc->fsEntryPoint, NS::UTF8StringEncoding));
    }
    
    // --- create PSO
    MTL::RenderPipelineDescriptor* psoDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    psoDesc->setVertexFunction(vs);
    psoDesc->setFragmentFunction(fs);
    
    rgAssert(renderStateDesc != nullptr);
    for(rgInt i = 0; i < kMaxColorAttachments; ++i)
    {
        TinyImageFormat colorAttFormat = renderStateDesc->colorAttachments[i].pixelFormat;
        rgBool blendingEnabled = renderStateDesc->colorAttachments[i].blendingEnabled;
        if(colorAttFormat != TinyImageFormat_UNDEFINED)
        {
            psoDesc->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)toMTLPixelFormat(colorAttFormat));
            psoDesc->colorAttachments()->object(i)->setBlendingEnabled(blendingEnabled);
            if(blendingEnabled)
            {
                psoDesc->colorAttachments()->object(i)->setRgbBlendOperation(MTL::BlendOperationAdd);
                psoDesc->colorAttachments()->object(i)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
                psoDesc->colorAttachments()->object(i)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
                
                psoDesc->colorAttachments()->object(i)->setAlphaBlendOperation(MTL::BlendOperationAdd);
                psoDesc->colorAttachments()->object(i)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
                psoDesc->colorAttachments()->object(i)->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
            }
        }
    }

    if(renderStateDesc->depthStencilAttachmentFormat != TinyImageFormat_UNDEFINED)
    {
        psoDesc->setDepthAttachmentPixelFormat((MTL::PixelFormat)toMTLPixelFormat(renderStateDesc->depthStencilAttachmentFormat));
    }
    
    graphicsPSO->mtlPSO = mtl()->device->newRenderPipelineState(psoDesc, &err);
    
    if(err)
    {
        printf("%s\n", err->localizedDescription()->utf8String());
        rgAssert(!"newRenderPipelineState error");
    }
    
    psoDesc->release();
    fs->release();
    vs->release();
    shaderLib->release();
    
    arp->release();
    
    return graphicsPSO;
}

void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso)
{
    pso->mtlPSO->release();
    rgDelete(pso);
}

GfxTexture2DRef creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
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
    GfxTexture2DRef t2dRef = eastl::shared_ptr<GfxTexture2D>(rgNew(GfxTexture2D), deleterGfxTexture2D);
    t2dRef->width = width;
    t2dRef->height = height;
    t2dRef->pixelFormat = format;
    t2dRef->mtlTexture = (__bridge MTL::Texture*)mtlTexture;
    name != nullptr ? strcpy(t2dRef->name, name) : strcpy(t2dRef->name, "[NoName]");
    
    return t2dRef;
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{
    t2d->mtlTexture->release();
    rgDelete(t2d);
}

void gfxHandleRenderCmdRenderPass(void const* cmd)
{
    GfxRenderPass* pass = &((RenderCmdRenderPass*)cmd)->renderPass;
    
    // create RenderCommandEncoder
    MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

    for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    {
        if(pass->colorAttachments[c].texture == kUninitializedHandle)
        {
            continue;
        }
        
        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][c];
        
        colorAttachmentDesc.texture = getMTLTexture(pass->colorAttachments[c].texture);
        colorAttachmentDesc.clearColor = toMTLClearColor(&pass->colorAttachments[c].clearColor);
        colorAttachmentDesc.loadAction = toMTLLoadAction(pass->colorAttachments[c].loadAction);
        colorAttachmentDesc.storeAction = toMTLStoreAction(pass->colorAttachments[c].storeAction);
    }
    
    MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
    depthAttachmentDesc.texture  = getMTLTexture(pass->depthStencilAttachmentTexture);
    depthAttachmentDesc.clearDepth = pass->clearDepth;
    depthAttachmentDesc.loadAction = toMTLLoadAction(pass->depthStencilAttachmentLoadAction);
    depthAttachmentDesc.storeAction = toMTLStoreAction(pass->depthStencilAttachmentStoreAction);

    mtl()->renderEncoder = [mtlCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    
    [renderPassDesc autorelease];
    
    
    MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDesc.depthWriteEnabled = true;
    id<MTLDepthStencilState> dsState = [getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    [depthStencilDesc release];
    
    [mtlRenderEncoder() setDepthStencilState:dsState];
    
    for(GfxTexture2DRef tex2D : gfxCtx()->texture2dManager)
    {
        [mtlRenderEncoder() useResource:(__bridge id<MTLTexture>)tex2D->mtlTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
    }
    
    [mtlRenderEncoder() setFragmentBuffer:getActiveMTLBuffer(mtl()->largeArrayTex2DArgBuffer) offset:0 atIndex:3];
}

void gfxHandleRenderCmdTexturedQuads(void const* cmd)
{
    //
    RenderCmdTexturedQuads* rc = (RenderCmdTexturedQuads*)cmd;
    
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
    
    [mtlRenderEncoder() setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)rc->pso->mtlPSO];
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

RG_END_NAMESPACE
#endif
