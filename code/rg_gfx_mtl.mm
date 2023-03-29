#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"
#include "metal_utils.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>

RG_BEGIN_NAMESPACE

#include "shaders/metal/imm_shader.inl"
#include "shaders/metal/simple2d_shader.inl"

#if 0
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

    metalutils::initMetal(ctx);
    
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
    TexturePtr birdTex = rg::loadTexture("bird_texture.png");
    gfxCtx()->mtl.birdTexture = gfxNewTexture2D(birdTex, GfxResourceUsage_Dynamic);
    
    for(rgInt i = 1; i <= 16; ++i)
    {
        char path[256];
        snprintf(path, 256, "textureslice/textureSlice %d.png", i);
        GfxTexture2DRef t2dptr = gfxNewTexture2D(rg::loadTexture(path), GfxResourceUsage_Static);
    }

    MTLArgumentDescriptor* argDesc = [MTLArgumentDescriptor argumentDescriptor];
    argDesc.index = 0;
    argDesc.dataType = MTLDataTypeTexture;
    argDesc.arrayLength = 100000;
    argDesc.textureType = MTLTextureType2D;
    
    gfxCtx()->mtl.largeArrayTex2DArgEncoder = (__bridge MTL::ArgumentEncoder*)[getMTLDevice() newArgumentEncoderWithArguments: @[argDesc]];
    gfxCtx()->mtl.largeArrayTex2DArgBuffer = gfxNewBuffer(nullptr, mtl()->largeArrayTex2DArgEncoder->encodedLength(), GfxResourceUsage_Dynamic);

    return 0;
}

rgInt gfxDraw()
{
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    // --- Autorelease pool BEGIN
    GfxCtx* ctx = gfxCtx();
    CA::MetalDrawable* currentMetalDrawable = metalutils::nextDrawable(ctx);
    rgAssert(currentMetalDrawable != nullptr);
    if(currentMetalDrawable != nullptr)
    {
        id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)mtl()->commandQueue->commandBuffer();
        
        // create RenderCommandEncoder
        MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][0];
        colorAttachmentDesc.texture = (__bridge id<MTLTexture>)(currentMetalDrawable->texture());
        colorAttachmentDesc.clearColor = MTLClearColorMake(0.5, 0.5, 0.5, 1.0);
        colorAttachmentDesc.loadAction = MTLLoadActionClear;
        colorAttachmentDesc.storeAction = MTLStoreActionStore;
        
        mtl()->currentRenderEncoder = (__bridge MTL::RenderCommandEncoder*)[commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
        [renderPassDesc autorelease];
        
        {
            // bind all textures
            mtl()->largeArrayTex2DArgEncoder->setArgumentBuffer(gfxCtx()->mtl.largeArrayTex2DArgBuffer->mtlBuffer, 0);
            
            for(rgInt i = 1; i <= 16 /*gfxCtx()->textures2D.size()*/; ++i)
            {
                char path[256];
                snprintf(path, 256, "textureslice/textureSlice %d.png", i);
                
                GfxTexture2DPtr tex = gfxGetTexture2DPtr(rgCRC32(path));
                if(tex)
                {
                    mtl()->largeArrayTex2DArgEncoder->setTexture(tex->mtlTexture, (i - 1) * 1000);
                    mtl()->currentRenderEncoder->useResource(tex->mtlTexture, MTL::ResourceUsageRead);
                }
            }

            mtl()->currentRenderEncoder->setFragmentBuffer(mtl()->largeArrayTex2DArgBuffer->mtlBuffer, 0, 3);
        }
        
        gfxGetRenderCmdList()->draw();
        gfxGetRenderCmdList()->afterDraw();
        
        
        /*
        renderCmdEnc->setRenderPipelineState(ctx->mtl.immPSO->mtlPSO);
        renderCmdEnc->setVertexBuffer(ctx->mtl.immVertexBuffer->mtlBuffer, 0, 0);
        renderCmdEnc->setFragmentTexture(gfxCtx()->mtl.birdTexture->mtlTexture, 0);
        renderCmdEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        */
        
        mtl()->currentRenderEncoder->endEncoding();
        [commandBuffer presentDrawable:(__bridge id<MTLDrawable>)currentMetalDrawable];
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

/*
MTL::ResourceOptions toMTLResourceOptions(GfxMemoryUsage usage)
{
    MTL::ResourceOptions mode; // todo: set default
    switch(usage)
    {
        case GfxMemoryUsage_CPUToGPU:
            mode = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
            break;
        case GfxMemoryUsage_GPUOnly:
            mode = MTL::StorageModePrivate;
            break;
        case GfxMemoryUsage_CPUOnly:
            rgAssert(!"Not implemented");
            break;
        default:
            rgAssert(!"Incorrect usages mode");
    }
    return mode;
}
*/

GfxBuffer* gfxNewBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    GfxBuffer* resource = new GfxBuffer;
    resource->usageMode = usage;
    resource->capacity = size;
    
    if(usage == GfxResourceUsage_Static)
    {
        rgAssert(data != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage);
    
    if(data != NULL)
    {
        resource->mtlBuffer = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithBytes:data length:(NSUInteger)size options:options];
    }
    else
    {
        resource->mtlBuffer = (__bridge MTL::Buffer*)[getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
    }
    
    return resource;
}

void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset)
{
    rgAssert(dstBuffer);
    
    if(dstBuffer->usageMode != GfxResourceUsage_Static)
    {
        memcpy((rgU8*)dstBuffer->mtlBuffer->contents() + offset, data, length);
    }
    else
    {
        rgAssert(!"Unimplemented or incorrect usages mode");
    }
}

void gfxDeleteBuffer(GfxBuffer* bufferResource)
{
    bufferResource->mtlBuffer->release();
}

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* graphicsPSO = new GfxGraphicsPSO;
    
    NS::AutoreleasePool* arp = NS::AutoreleasePool::alloc()->init();
    
    // --- compile shader
    MTL::CompileOptions* compileOptions = MTL::CompileOptions::alloc()->init();
    compileOptions->setPreprocessorMacros(metalutils::getPreprocessorMacrosDict(shaderDesc->macros));
    
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

GfxTexture2DRef gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage)
{
    Texture* tex = texture.get();
    return gfxNewTexture2D(tex->hash, tex->buf, tex->width, tex->height, tex->format, usage, tex->name);
}

GfxTexture2DRef gfxNewTexture2D(rgHash hash, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage, char const* name)
{
    rgAssert(buf != NULL);
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = MTLTextureType2D;
    texDesc.storageMode = MTLStorageModeShared;
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
    GfxTexture2DRef t2dPtr = eastl::shared_ptr<GfxTexture2D>(rgNew(GfxTexture2D), gfxDeleteTexture2D);
    t2dPtr->width = width;
    t2dPtr->height = height;
    t2dPtr->pixelFormat = format;
    t2dPtr->mtlTexture = (__bridge MTL::Texture*)mtlTexture;
    name != nullptr ? strcpy(t2dPtr->name, name) : strcpy(t2dPtr->name, "[NoName]");

    gfxCtx()->textures2D.insert(eastl::make_pair(hash, t2dPtr));
    
    return t2dPtr;
}

void gfxDeleteTexture2D(GfxTexture2D* t2d)
{
    t2d->mtlTexture->release();
    rgDelete(t2d);
}

void gfxHandleRenderCmdTexturedQuad(void const* cmd)
{

}

void gfxHandleRenderCmdTexturedQuads(void const* cmd)
{
    //
    RenderCmdTexturedQuads* rc = (RenderCmdTexturedQuads*)cmd;
    
    eastl::vector<SimpleVertexFormat> vertices;
    genTexturedQuadVertices(rc->quads, &vertices);
    
    // Later only one texture per RenderCmdTexturedQuads allowed
    struct InstanceParams
    {
        rgU32 texID;
    };
    // this is same as as rgU32, so we can write
    eastl::fixed_vector<rgU32, 32> instanceParamsArray = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    
    
    GfxCtx* ctx = gfxCtx();
    
    rgFloat* om = toFloatPtr(ctx->orthographicMatrix);
    
    mtl()->currentRenderEncoder->setRenderPipelineState(ctx->mtl.simple2dPSO->mtlPSO);
    mtl()->currentRenderEncoder->setVertexBytes(om, 16 * sizeof(rgFloat), 0);
    mtl()->currentRenderEncoder->setVertexBytes(&vertices.front(), vertices.size() * sizeof(SimpleVertexFormat), 1);
    mtl()->currentRenderEncoder->setCullMode(MTL::CullModeNone);
    
    MTL::Viewport viewport;
    viewport.originX = 0;
    viewport.originY = 0;
    viewport.width = 720;
    viewport.height = 720;
    viewport.znear = 0.0f;
    viewport.zfar = 1000.0f;
    mtl()->currentRenderEncoder->setViewport(viewport);
    //ctx->mtl.activeRCEncoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    //ctx->mtl.activeRCEncoder->setFragmentTexture(gfxCtx()->mtl.birdTexture->mtlTexture, 0);
    mtl()->currentRenderEncoder->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), 6, NS::UInteger(vertices.size())/6);
}

RG_END_NAMESPACE
#endif
