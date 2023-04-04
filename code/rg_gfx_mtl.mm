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
#include "shaders/metal/simple2d_shader.inl"

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

MTLPixelFormat toMTLPixelFormat(TinyImageFormat fmt)
{
    return (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(fmt);
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
    //
    CAMetalLayer* mtlLayer = (CAMetalLayer*)ctx->mtl.layer;
    mtlLayer.device = (__bridge id<MTLDevice>)(ctx->mtl.device);
    mtlLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtlLayer.maximumDrawableCount = RG_MAX_FRAMES_IN_FLIGHT;
    //

    mtl()->framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);
    
    GfxShaderDesc immShaderDesc = {};
    immShaderDesc.shaderSrcCode = g_ImmShaderSrcCode;
    immShaderDesc.vsEntryPoint = "immVS";
    immShaderDesc.fsEntryPoint = "immFS";
    immShaderDesc.macros = "IMM_FORCE_RED0 IMM_FORCE_UV_OUTPUT0 DUDLEY_IS_A_GIT";
    
    GfxRenderStateDesc immRenderStateDesc = {};
    immRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    
    gfxCtx()->mtl.immPSO = gfxNewGraphicsPSO(&immShaderDesc, &immRenderStateDesc);
    
    //
    
    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrcCode = g_Simple2DShaderSrcCode;
    simple2dShaderDesc.vsEntryPoint = "simple2d_VS";
    simple2dShaderDesc.fsEntryPoint = "simple2d_FS";
    simple2dShaderDesc.macros = "F";
    
    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    
    gfxCtx()->mtl.simple2dPSO = gfxNewGraphicsPSO(&simple2dShaderDesc, &simple2dRenderStateDesc);
    
    //
    SimpleVertexFormat1 triangleVertices[3] =
    {
        {{-0.8f, 0.8f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, -0.8f, 0.0f}, {0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.8f,  0.8f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };
    
    rgUInt triangleVerticesSize = sizeof(triangleVertices);
    
    gfxCtx()->mtl.immVertexBuffer = gfxNewBuffer(nullptr, triangleVerticesSize, GfxResourceUsage_Dynamic);
    gfxUpdateBuffer(gfxCtx()->mtl.immVertexBuffer, triangleVertices, triangleVerticesSize, 0);

    //metalutils::getPreprocessorMacrosDict("SHADOW_PASS, USE_TEX1  USE_TEX2");

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
    gfxCtx()->frameIndex = (gfxCtx()->frameIndex + 1) % RG_MAX_FRAMES_IN_FLIGHT;
    
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    // --- Autorelease pool BEGIN
    GfxCtx* ctx = gfxCtx();
    //CA::MetalDrawable* currentMetalDrawable = metalutils::nextDrawable(ctx);
    CAMetalLayer* metalLayer = (CAMetalLayer*)mtl()->layer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    
    rgAssert(metalDrawable != nullptr);
    if(metalDrawable != nullptr)
    {
        id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)mtl()->commandQueue->commandBuffer();
        
        // create RenderCommandEncoder
        MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][0];
        //colorAttachmentDesc.texture = (__bridge id<MTLTexture>)(mtlDrawable->texture());
        colorAttachmentDesc.texture = [metalDrawable texture];
        colorAttachmentDesc.clearColor = MTLClearColorMake(0.5, 0.5, 0.5, 1.0);
        colorAttachmentDesc.loadAction = MTLLoadActionClear;
        colorAttachmentDesc.storeAction = MTLStoreActionStore;
        
        mtl()->currentRenderEncoder = (__bridge MTL::RenderCommandEncoder*)[commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
        [renderPassDesc autorelease];
        
        // bind all textures
        {
            rgInt argBufferActiveIndex = mtl()->largeArrayTex2DArgBuffer->activeIdx;
            MTL::Buffer* argBuffer = mtl()->largeArrayTex2DArgBuffer->mtlBuffers[argBufferActiveIndex];
            
            mtl()->largeArrayTex2DArgEncoder->setArgumentBuffer(argBuffer, 0);
            
            rgSize largeArrayTex2DIndex = 0;
            
#if 0
            for(GfxTexture2DHandle handle : gfxCtx()->debugTextureHandles)
            {
                GfxTexture2DPtr texPtr = gfxGetTexture2DPtr(handle);
                mtl()->largeArrayTex2DArgEncoder->setTexture(texPtr->mtlTexture, largeArrayTex2DIndex * 6000);
                // TODO: allocate textures from a heap
                mtl()->currentRenderEncoder->useResource(texPtr->mtlTexture, MTL::ResourceUsageRead);
                
                ++largeArrayTex2DIndex;
            }
#else
            for(GfxTexture2DRef tex2D : gfxCtx()->textures2D)
            {
                mtl()->largeArrayTex2DArgEncoder->setTexture(tex2D->mtlTexture, largeArrayTex2DIndex);
                mtl()->currentRenderEncoder->useResource(tex2D->mtlTexture, MTL::ResourceUsageRead);
                ++largeArrayTex2DIndex;
            }
#endif
            argBuffer->didModifyRange(NS::Range(0, argBuffer->length()));
            mtl()->currentRenderEncoder->setFragmentBuffer(argBuffer, 0, 3);
        }
        
        gfxGetRenderCmdList()->draw();
        gfxGetRenderCmdList()->afterDraw();

        mtl()->currentRenderEncoder->endEncoding();
        [commandBuffer presentDrawable:metalDrawable];
        
        __block dispatch_semaphore_t blockSemaphore = mtl()->framesInFlightSemaphore;
        [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
         {
            dispatch_semaphore_signal(blockSemaphore);
        }];
        
        [commandBuffer commit];
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

GfxBuffer* gfxNewBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    GfxBuffer* resource = new GfxBuffer;
    resource->usageMode = usage;
    resource->size = size;
    resource->activeIdx = 0;
    
    if(usage == GfxResourceUsage_Static)
    {
        rgAssert(data != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if(data != NULL)
        {
            resource->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithBytes:data length:(NSUInteger)size options:options];
        }
        else
        {
            resource->mtlBuffers[i] = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        
        if(usage == GfxResourceUsage_Static)
        {
            break;
        }
    }
    
    return resource;
}

void gfxUpdateBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset)
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

void gfxDeleteBuffer(GfxBuffer* buffer)
{
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        buffer->mtlBuffers[i]->release();
        
        if(buffer->usageMode == GfxResourceUsage_Static)
        {
            break;
        }
    }
}

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* graphicsPSO = new GfxGraphicsPSO;
    
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
    for(rgInt i = 0; i < GfxRenderStateDesc::MAX_COLOR_ATTACHMENTS; ++i)
    {
        TinyImageFormat colorAttFormat = renderStateDesc->colorAttachments[i].pixelFormat;
        if(colorAttFormat != TinyImageFormat_UNDEFINED)
        {
            psoDesc->colorAttachments()->object(i)->setPixelFormat((MTL::PixelFormat)toMTLPixelFormat(colorAttFormat));
        }
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
}

GfxTexture2DRef creatorGfxTexture2D(GfxTexture2DHandle handle, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage, char const* name)
{
    rgAssert(buf != NULL);
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = MTLTextureType2D;
    texDesc.storageMode = MTLStorageModeManaged;
    texDesc.usage = MTLTextureUsageShaderRead; // TODO: RenderTarget
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
    t2dRef->handle = handle;
    name != nullptr ? strcpy(t2dRef->name, name) : strcpy(t2dRef->name, "[NoName]");
    
    return t2dRef;
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{
    t2d->mtlTexture->release();
    rgDelete(t2d);
}

void gfxHandleRenderCmdTexturedQuads(void const* cmd)
{
    //
    RenderCmdTexturedQuads* rc = (RenderCmdTexturedQuads*)cmd;
    
    eastl::vector<SimpleVertexFormat> vertices;
    eastl::vector<SimpleInstanceParams> instanceParams;
    
    genTexturedQuadVertices(rc->quads, &vertices, &instanceParams);
    
    if(gfxCtx()->rcTexturedQuadsVB == nullptr)
    {
        gfxCtx()->rcTexturedQuadsVB = gfxNewBuffer(nullptr, rgMEGABYTE(16), GfxResourceUsage_Stream);
    }
    
    if(gfxCtx()->rcTexturedQuadsInstParams == nullptr)
    {
        gfxCtx()->rcTexturedQuadsInstParams = gfxNewBuffer(nullptr, rgMEGABYTE(4), GfxResourceUsage_Stream);
    }
    
    GfxBuffer* texturesQuadVB = gfxCtx()->rcTexturedQuadsVB;
    GfxBuffer* texturedQuadInstParams = gfxCtx()->rcTexturedQuadsInstParams;
    gfxUpdateBuffer(texturesQuadVB, &vertices.front(), vertices.size() * sizeof(SimpleVertexFormat), 0);
    gfxUpdateBuffer(texturedQuadInstParams, &instanceParams.front(), instanceParams.size() * sizeof(SimpleInstanceParams), 0);
    
    // this is same as as rgU32, so we can write
    //eastl::fixed_vector<rgU32, 32> instanceParamsArray = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    

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
    
    mtl()->currentRenderEncoder->setRenderPipelineState(ctx->mtl.simple2dPSO->mtlPSO);
    mtl()->currentRenderEncoder->setVertexBytes(&cam, sizeof(Camera), 0);
    mtl()->currentRenderEncoder->setFragmentBuffer(texturedQuadInstParams->mtlBuffers[texturedQuadInstParams->activeIdx], 0, 4);
    mtl()->currentRenderEncoder->setVertexBuffer(texturesQuadVB->mtlBuffers[texturesQuadVB->activeIdx], 0, 1);
    mtl()->currentRenderEncoder->setCullMode(MTL::CullModeNone);
    
    MTL::Viewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = 720;
    viewport.height = 720;
    viewport.znear = 0.0;
    viewport.zfar = 1.0;
    mtl()->currentRenderEncoder->setViewport(viewport);
    //ctx->mtl.activeRCEncoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    //ctx->mtl.activeRCEncoder->setFragmentTexture(gfxCtx()->mtl.birdTexture->mtlTexture, 0);
    mtl()->currentRenderEncoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), 6, NS::UInteger(vertices.size())/6);
}

RG_END_NAMESPACE
#endif
