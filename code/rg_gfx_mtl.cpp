#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"
#include "metal_utils.h"

#include <simd/simd.h>
RG_BEGIN_NAMESPACE

#define mtlCtx g_GfxCtx.mtl

#include "shaders/metal/imm_shader.inl"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace metal_util
{
MTL::PixelFormat convertPixelFormat(TinyImageFormat f)
{
    return (MTL::PixelFormat)TinyImageFormat_ToMTLPixelFormat(f);
}

MTL::ResourceUsage convertResourceUsage(GfxResourceUsage usage)
{
    switch(usage)
    {
        case GfxResourceUsage_Read:
            return MTL::ResourceUsageRead;
            break;
            
        case GfxResourceUsage_Write:
            return MTL::ResourceUsageWrite;
            break;
            
        case GfxResourceUsage_ReadWrite:
            return (MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
            break;
            
        default:
            return MTL::ResourceUsageRead;
            break;
    }
}
}

static MTL::Device* mtlDevice()
{
    return gfxCtx()->mtl.device;
}

struct SimpleVertexFormat
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
    
    SimpleVertexFormat triangleVertices[3] =
    {
        {{-0.8f, 0.8f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.0f, -0.8f, 0.0f}, {0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{0.8f,  0.8f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };
    
    rgUInt triangleVerticesSize = sizeof(triangleVertices);
    
    gfxCtx()->mtl.immVertexBuffer = gfxNewBuffer(nullptr, triangleVerticesSize, GfxMemoryUsage_CPUToGPU);
    gfxUpdateBuffer(gfxCtx()->mtl.immVertexBuffer, triangleVertices, triangleVerticesSize);

    //metalutils::getPreprocessorMacrosDict("SHADOW_PASS, USE_TEX1  USE_TEX2");
    gfxCtx()->mtl.birdTexture = gfxNewTexture2D("bird_texture.png", GfxResourceUsage_Read);
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
        MTL::CommandBuffer* commandBuffer = ctx->mtl.commandQueue->commandBuffer();
        // create renderpass descriptor
        MTL::RenderPassDescriptor* renderPassDesc = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* colorAttachmentDesc = renderPassDesc->colorAttachments()->object(0);
        colorAttachmentDesc->setClearColor(MTL::ClearColor::Make(0.5, 0.5, 0.5, 1.0));
        colorAttachmentDesc->setLoadAction(MTL::LoadActionClear);
        colorAttachmentDesc->setStoreAction(MTL::StoreActionStore);
        colorAttachmentDesc->setTexture(currentMetalDrawable->texture());
        MTL::RenderCommandEncoder* renderCmdEnc = commandBuffer->renderCommandEncoder(renderPassDesc);
        renderPassDesc->autorelease();
        renderCmdEnc->setRenderPipelineState(ctx->mtl.immPSO->mtlPSO);
        renderCmdEnc->setVertexBuffer(ctx->mtl.immVertexBuffer->mtlBuffer, 0, 0);
        renderCmdEnc->setFragmentTexture(gfxCtx()->mtl.birdTexture->mtlTexture, 0);
        renderCmdEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        
        renderCmdEnc->endEncoding();
        commandBuffer->presentDrawable(currentMetalDrawable);
        commandBuffer->commit();
    }
    else
    {
        printf("Current drawable is null!!!");
        // exit
    }
    // --- Autorelease pool END
    arp->release();
    
    return 0;
}

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

GfxBuffer* gfxNewBuffer(void* data, rgU32 length, GfxMemoryUsage usage)
{
    MTL::ResourceOptions mode = toMTLResourceOptions(usage);
    
    GfxBuffer* resource = new GfxBuffer;
    resource->usageMode = usage;
    resource->capacity = length;
    
    if(data != nullptr)
    {
        resource->mtlBuffer = gfxCtx()->mtl.device->newBuffer(data, NS::UInteger(length), mode);
    }
    else
    {
        resource->mtlBuffer = gfxCtx()->mtl.device->newBuffer(NS::UInteger(length), mode);
    }
    return resource;
}

void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset)
{
    rgAssert(dstBuffer);
    
    if(dstBuffer->usageMode == GfxMemoryUsage_CPUToGPU)
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
    
    MTL::Library* shaderLib = mtlDevice()->newLibrary(NS::String::string(shaderDesc->shaderSrcCode, NS::UTF8StringEncoding), compileOptions, &err);
    
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
            psoDesc->colorAttachments()->object(i)->setPixelFormat(metal_util::convertPixelFormat(colorAttFormat));
        }
    }
    
    graphicsPSO->mtlPSO = mtlDevice()->newRenderPipelineState(psoDesc, &err);
    
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

GfxTexture2D* gfxNewTexture2D(char const* filename, GfxResourceUsage usage)
{
    rgInt texWidth, texHeight, texChnl;
    unsigned char* texData = stbi_load(filename, &texWidth, &texHeight, &texChnl, 4);
    
    MTL::TextureDescriptor* texDesc = MTL::TextureDescriptor::alloc()->init();
    
    texDesc->setWidth(texWidth);
    texDesc->setHeight(texHeight);
    texDesc->setPixelFormat(metal_util::convertPixelFormat(TinyImageFormat_R8G8B8A8_UNORM));
    texDesc->setTextureType(MTL::TextureType2D);
    texDesc->setStorageMode(MTL::StorageModeShared);
    texDesc->setUsage(metal_util::convertResourceUsage(usage));
    
    GfxTexture2D* texture2D = new GfxTexture2D;
    
    texture2D->mtlTexture = mtlDevice()->newTexture(texDesc);
    texture2D->mtlTexture->replaceRegion(MTL::Region(0, 0, 0, texWidth, texHeight, 1), 0, texData, texWidth * 4);
    
    texDesc->release();
    stbi_image_free(texData);
    
    return texture2D;
}

void gfxDeleteTexture2D(GfxTexture2D* t2d)
{
    t2d->mtlTexture->release();
}

RG_END_NAMESPACE
#endif
