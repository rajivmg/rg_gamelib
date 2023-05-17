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

MTLResourceOptions toMTLResourceOptions(GfxBufferUsage usage, rgBool dynamic)
{
    MTLResourceOptions options = MTLResourceStorageModeManaged;
    if(dynamic)
    {
        options |= MTLResourceCPUCacheModeWriteCombined;
    }
    return options;

    /*
    // TODO: recheck the values
    switch(usage)
    {
        case GfxBufferUsage_VertexBuffer:
        case GfxBufferUsage_IndexBuffer:
        case GfxBufferUsage_StructuredBuffer:
        {
            return MTLResourceStorageModeManaged;
        } break;
            
        case GfxBufferUsage_ShaderRW:
        case GfxBufferUsage_ConstantBuffer:
        {
            return MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;
        } break;
            
        default:
            rgAssert(!"Invalid resource usage option");
    }
     */
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

id<MTLRenderCommandEncoder> mtlRenderCommandEncoder(void* ptr)
{
    return (__bridge id<MTLRenderCommandEncoder>)ptr;
}

struct SimpleVertexFormat1
{
    simd::float3 position;
    simd::float2 texcoord;
    simd::float4 color;
};

id<MTLComputePipelineState> histogramComputePipeline;

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
    
    [computeHistogram release];
    [histogramLibrary release];
    
    gfx::createTexture2D("histogramTest", rg::loadTexture("histogram_test.png"), false, GfxTextureUsage_ShaderRead);
    gfx::createBuffer("histogramBuffer", nullptr, sizeof(rgUInt)*255*3, GfxBufferUsage_ShaderRW, false);
}

void testComputeAtomicsRun()
{
    id<MTLComputeCommandEncoder> computeEncoder = [mtlCommandBuffer() computeCommandEncoder];
    [computeEncoder setComputePipelineState:histogramComputePipeline];
    [computeEncoder setTexture:getMTLTexture(gfx::findTexture2D("histogramTest")) atIndex:0];
    [computeEncoder setBuffer:getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) offset:0 atIndex:0];
    //[computeEncoder setTexture:(id<MTLTexture>)getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) atIndex:1];
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
        gfx::renderTarget[i] = createTexture2D(tag.c_str(), nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, false, GfxTextureUsage_RenderTarget);
    }
    gfx::depthStencilBuffer = createTexture2D("depthStencilBuffer", nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D16_UNORM, false, GfxTextureUsage_DepthStencil);
    
    MTLArgumentDescriptor* argDesc = [MTLArgumentDescriptor argumentDescriptor];
    argDesc.index = 0;
    argDesc.dataType = MTLDataTypeTexture;
    argDesc.arrayLength = 99999;
    argDesc.textureType = MTLTextureType2D;
    
    MTLArgumentDescriptor* argDesc2 = [MTLArgumentDescriptor argumentDescriptor];
    argDesc2.index = 90001;
    argDesc2.dataType = MTLDataTypePointer;
    
    mtl->largeArrayTex2DArgEncoder = (__bridge MTL::ArgumentEncoder*)[getMTLDevice() newArgumentEncoderWithArguments: @[argDesc, argDesc2]];
    mtl->largeArrayTex2DArgBuffer = createBuffer("largeArrayTex2DArgBuffer", nullptr, mtl->largeArrayTex2DArgEncoder->encodedLength(), GfxBufferUsage_ConstantBuffer, true);

    testComputeAtomicsSetup();
    return 0;
}

void destroy()
{
    
}

void startNextFrame()
{
    dispatch_semaphore_wait(mtl->framesInFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    g_FrameIndex = (g_FrameIndex + 1) % RG_MAX_FRAMES_IN_FLIGHT;
    
    currentRenderPass = nullptr;
    currentRenderCmdEncoder = nullptr;
    
    // Autorelease pool BEGIN
    mtl->autoReleasePool = NS::AutoreleasePool::alloc()->init();
    
    
    CAMetalLayer* metalLayer = (CAMetalLayer*)mtl->layer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    rgAssert(metalDrawable != nullptr);
    mtl->caMetalDrawable = metalDrawable;
    
    mtl->commandBuffer = (__bridge id<MTLCommandBuffer>)mtl->commandQueue->commandBuffer();
    
    //
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
    //
}

void endFrame()
{
    //testComputeAtomicsRun();
    
    // TODO: Add assert if the currentRenderCmdEncoder is not ended
    //[gfx::mtlRenderCommandEncoder(currentRenderCmdEncoder->renderCmdEncoder) endEncoding];
    
    // blit renderTarget0 to MTLDrawable
    id<MTLBlitCommandEncoder> blitEncoder = [mtlCommandBuffer() blitCommandEncoder];
    
    id<MTLTexture> srcTexture = getMTLTexture(gfx::renderTarget[g_FrameIndex]);
    id<MTLTexture> dstTexture = ((__bridge id<CAMetalDrawable>)mtl->caMetalDrawable).texture;
    [blitEncoder copyFromTexture:srcTexture toTexture:dstTexture];
    [blitEncoder endEncoding];
    
    [mtlCommandBuffer() presentDrawable:((__bridge id<CAMetalDrawable>)mtl->caMetalDrawable)];
    
    __block dispatch_semaphore_t blockSemaphore = mtl->framesInFlightSemaphore;
    [mtlCommandBuffer() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
     {
        dispatch_semaphore_signal(blockSemaphore);
    }];
    
    [mtlCommandBuffer() commit];
    
    // Autorelease pool END
    mtl->autoReleasePool->release();
    
    return 0;
}

void onSizeChanged()
{
    
}

void creatorGfxBuffer(char const* tag, void* buf, rgU32 size, GfxBufferUsage usage, rgBool dynamic, GfxBuffer* obj)
{
    if(dynamic == false && usage != GfxBufferUsage_ShaderRW)
    {
        rgAssert(buf != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage, dynamic);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        id<MTLBuffer> mtlBuffer;
        if(buf != NULL)
        {
            mtlBuffer = [getMTLDevice() newBufferWithBytes:buf length:(NSUInteger)size options:options];
        }
        else
        {
            mtlBuffer = [getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        mtlBuffer.label = [NSString stringWithUTF8String:tag];
        
        obj->mtlBuffers[i] = (__bridge MTL::Buffer*)mtlBuffer;
        
        if(dynamic == false)
        {
            break;
        }
    }
}

void updaterGfxBuffer(void* data, rgUInt size, rgUInt offset, GfxBuffer* buffer)
{
    rgAssert(buffer);
    
    if(buffer->dynamic == true)
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
        
        if(obj->dynamic == false)
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
        psoDesc.label = [NSString stringWithUTF8String:tag];
        
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
        id<MTLRenderPipelineState> pso = [getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo reflection:&reflectionInfo error:&err];
        
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newRenderPipelineState error");
        }
        
        obj->mtlPSO = pso;
        
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

void creatorGfxTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj)
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
    mtlTexture.label = [NSString stringWithUTF8String:tag];
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

RG_GFX_END_NAMESPACE

void GfxRenderCmdEncoder::begin(GfxRenderPass* renderPass)
{
    // create RenderCommandEncoder
    MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

    for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    {
        if(renderPass->colorAttachments[c].texture == NULL)
        {
            continue;
        }
        
        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][c];
        GfxColorAttachmentDesc* colorAttachment = &renderPass->colorAttachments[c];
        
        colorAttachmentDesc.texture     = gfx::getMTLTexture(colorAttachment->texture);
        colorAttachmentDesc.loadAction  = gfx::toMTLLoadAction(colorAttachment->loadAction);
        colorAttachmentDesc.storeAction = gfx::toMTLStoreAction(colorAttachment->storeAction);
        colorAttachmentDesc.clearColor  = gfx::toMTLClearColor(&colorAttachment->clearColor);
    }
    MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
    depthAttachmentDesc.texture     = gfx::getMTLTexture(renderPass->depthStencilAttachmentTexture);
    depthAttachmentDesc.loadAction  = gfx::toMTLLoadAction(renderPass->depthStencilAttachmentLoadAction);
    depthAttachmentDesc.storeAction = gfx::toMTLStoreAction(renderPass->depthStencilAttachmentStoreAction);
    depthAttachmentDesc.clearDepth  = renderPass->clearDepth;

    id<MTLRenderCommandEncoder> mtlRenderEncoder = [gfx::mtlCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    [renderPassDesc autorelease];
    
    MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDesc.depthWriteEnabled = true;
    id<MTLDepthStencilState> dsState = [gfx::getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    [depthStencilDesc release];
    
    [mtlRenderEncoder setDepthStencilState:dsState];
    
    for(GfxTexture2D* texture2d : *gfx::bindlessManagerTexture2D)
    {
        if(texture2d != nullptr)
        {
            [mtlRenderEncoder useResource:(__bridge id<MTLTexture>)texture2d->mtlTexture usage:MTLResourceUsageRead stages:MTLRenderStageVertex|MTLRenderStageFragment];
        }
    }
    
    [mtlRenderEncoder setFragmentBuffer:gfx::getActiveMTLBuffer(gfx::mtl->largeArrayTex2DArgBuffer) offset:0 atIndex:gfx::kBindlessTextureSetBinding];
    
    renderCmdEncoder = (__bridge void*)mtlRenderEncoder;
}

void GfxRenderCmdEncoder::end()
{
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) endEncoding];
}

void GfxRenderCmdEncoder::pushDebugTag(const char* tag)
{
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) pushDebugGroup:[NSString stringWithUTF8String:tag]];
}

void GfxRenderCmdEncoder::setViewport(rgFloat4 viewport)
{
    setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
}

void GfxRenderCmdEncoder::setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height)
{
    MTLViewport vp;
    vp.originX = originX;
    vp.originY = originY;
    vp.width   = width;
    vp.height  = height;
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setViewport:vp];
}

void GfxRenderCmdEncoder::setGraphicsPSO(GfxGraphicsPSO* pso)
{
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setRenderPipelineState:gfx::toMTLRenderPipelineState(pso)];
}

void GfxRenderCmdEncoder::drawTexturedQuads(TexturedQuads* quads)
{
    eastl::vector<gfx::SimpleVertexFormat> vertices;
    eastl::vector<gfx::SimpleInstanceParams> instanceParams;
    
    genTexturedQuadVertices(quads, &vertices, &instanceParams);
    
    if(gfx::rcTexturedQuadsVB == NULL)
    {
        gfx::rcTexturedQuadsVB = gfx::createBuffer("texturedQuadsVB", nullptr, rgMEGABYTE(16), GfxBufferUsage_VertexBuffer, true);
    }
    
    if(gfx::rcTexturedQuadsInstParams == NULL)
    {
        gfx::rcTexturedQuadsInstParams = gfx::createBuffer("texturedQuadInstParams", nullptr, rgMEGABYTE(4), GfxBufferUsage_StructuredBuffer, true);
    }
    
    GfxBuffer* texturesQuadVB = gfx::rcTexturedQuadsVB;
    GfxBuffer* texturedQuadInstParams = gfx::rcTexturedQuadsInstParams;
    updateBuffer("texturedQuadsVB", &vertices.front(), vertices.size() * sizeof(gfx::SimpleVertexFormat), 0);
    updateBuffer("texturedQuadInstParams", &instanceParams.front(), instanceParams.size() * sizeof(gfx::SimpleInstanceParams), 0);
    
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

    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setVertexBytes:&cam length:sizeof(Camera) atIndex:0];
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setFragmentBuffer:gfx::getActiveMTLBuffer(texturedQuadInstParams) offset:0 atIndex:4];
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:gfx::getActiveMTLBuffer(texturesQuadVB) offset:0 atIndex:1];
    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) setCullMode:MTLCullModeNone];

    [gfx::mtlRenderCommandEncoder(renderCmdEncoder) drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:vertices.size()/6];
}

RG_END_NAMESPACE
#endif
