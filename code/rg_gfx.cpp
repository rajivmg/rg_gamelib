//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>
#include <EASTL/string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_RG_NAMESPACE

// --- Game Graphics APIs
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };

TextureRef loadTexture(char const* filename)
{
    rgInt width, height, texChnl;
    unsigned char* texData = stbi_load(filename, &width, &height, &texChnl, 4);

    if(texData == NULL)
    {
        return nullptr;
    }

    //TextureRef tptr = eastl::make_shared<Texture>(unloadTexture);
    TextureRef tptr = eastl::shared_ptr<Texture>(rgNew(Texture), unloadTexture);
    strncpy(tptr->name, filename, rgARRAY_COUNT(Texture::name));
    tptr->name[rgARRAY_COUNT(Texture::name) - 1] = '\0';
    //strcpy(tptr->name, "[NONAME]");
    tptr->hash = rgCRC32(filename);
    tptr->width = width;
    tptr->height = height;
    tptr->format = TinyImageFormat_R8G8B8A8_UNORM;
    tptr->buf = texData;

    return tptr;
}

void unloadTexture(Texture* t)
{
    stbi_image_free(t->buf);
    rgDelete(t);
}

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx)
{
    QuadUV r;

    r.uvTopLeft[0] = (rgFloat)xPx / refWidthPx;
    r.uvTopLeft[1] = (rgFloat)yPx / refHeightPx;

    r.uvBottomRight[0] = (xPx + widthPx) / (rgFloat)refWidthPx;
    r.uvBottomRight[1] = (yPx + heightPx) / (rgFloat)refHeightPx;

    return r;
}

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture)
{
    return createQuadUV(xPx, yPx, widthPx, heightPx, refTexture->width, refTexture->height);
}

void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture2D* tex)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.pos = {posSize.x, posSize.y, 1.0f};
    q.size = posSize.zw;
    q.offsetOrientation = offsetOrientation;
    q.texID = gfx::bindlessManagerTexture2D->getBindlessIndex(tex);
}

RG_BEGIN_GFX_NAMESPACE

SDL_Window* mainWindow;
rgUInt frameNumber;

// CURRENT STATE
GfxRenderPass*          currentRenderPass;
GfxRenderCmdEncoder*    currentRenderCmdEncoder;
GfxBlitCmdEncoder*      currentBlitCmdEncoder;
GfxGraphicsPSO*         currentGraphicsPSO;

// OBJECT REGISTRIES
GfxObjectRegistry<GfxTexture2D>*    texture2D;
GfxObjectRegistry<GfxBuffer>*       buffer;
GfxObjectRegistry<GfxGraphicsPSO>*  graphicsPSO;
GfxObjectRegistry<GfxSampler>*      sampler;

GfxBindlessResourceManager<GfxTexture2D>*   bindlessManagerTexture2D;

// DEFAULT RESOURCES
GfxTexture2D*       renderTarget[RG_MAX_FRAMES_IN_FLIGHT];
GfxTexture2D*       depthStencilBuffer;
GfxSampler*         bilinearSampler;
GfxFrameAllocator*  frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];

// MISC
Matrix4 orthographicMatrix;
Matrix4 viewMatrix;
eastl::vector<GfxTexture2D*> debugTextureHandles; // test only


Matrix4 makeOrthoProjection(rgFloat left, rgFloat right, rgFloat bottom, rgFloat top, rgFloat nearValue, rgFloat farValue)
{
    rgFloat length = 1.0f / (right - left);
    rgFloat height = 1.0f / (top - bottom);
    rgFloat depth  = 1.0f / (farValue - nearValue);
    
    return Matrix4(Vector4(2.0f * length, 0, 0, 0),
                   Vector4(0, 2.0f * height, 0, 0),
                   Vector4(0, 0, depth, 0),
                   Vector4(-((right + left) * length), -((top +bottom) * height), -nearValue * depth, 1.0f));
}

Matrix4 createPerspectiveProjectionMatrix(rgFloat focalLength, rgFloat aspectRatio, rgFloat nearPlane, rgFloat farPlane)
{
#if defined(RG_METAL_RNDR) || defined(RG_D3D12_RNDR)
    return  Matrix4(Vector4(focalLength, 0.0f, 0.0f, 0.0f),
                    Vector4(0.0f, focalLength * aspectRatio, 0.0f, 0.0f),
                    Vector4(0.0f, 0.0f, -farPlane/(farPlane - nearPlane), -1.0f),
                    Vector4(0.0f, 0.0f, (-farPlane * nearPlane)/(farPlane - nearPlane), 0.0f));
#else
    rgAssert(!"Not defined");
#endif
}

rgInt preInit()
{
    gfx::buffer = rgNew(GfxObjectRegistry<GfxBuffer>);
    gfx::sampler = rgNew(GfxObjectRegistry<GfxSampler>);
    gfx::texture2D = rgNew(GfxObjectRegistry<GfxTexture2D>);
    gfx::graphicsPSO = rgNew(GfxObjectRegistry<GfxGraphicsPSO>);

    gfx::bindlessManagerTexture2D = rgNew(GfxBindlessResourceManager<GfxTexture2D>);
    
    return 0;
}

rgInt initCommonStuff()
{
    // Initialize frame buffer allocators
    for(rgS32 i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        frameAllocators[i] = rgNew(GfxFrameAllocator)(rgMEGABYTE(16));
    }
    
    //
    gfx::orthographicMatrix = Matrix4::orthographic(0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height, 0, 0.1f, 1000.0f);
    gfx::viewMatrix = Matrix4::lookAt(Point3(0, 0, 0), Point3(0, 0, -1000.0f), Vector3(0, 1.0f, 0));
    
#ifdef RG_METAL_RNDR
    //Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 shiftZHalf = Matrix4::translation(Vector3(0.0f, 0.0f, 0.5f));
    //Matrix4 shiftZHalf = Matrix4::translation(Vector3(1.0f, -1.0f, -1000.0f / (0.1f - 1000.0f)));
    //Matrix4 scaleShiftZHalf = shiftZHalf * scaleZHalf;
    //ctx->orthographicMatrix = shiftZHalf * scaleZHalf * ctx->orthographicMatrix;
    gfx::orthographicMatrix = makeOrthoProjection(0.0f, g_WindowInfo.width, g_WindowInfo.height, 0.0f, 0.1f, 1000.0f);
#endif
    
    gfx::sampler->create("bilinearSampler", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Nearest, false);
    
    return 0;
}

void atFrameStart()
{
    gfx::buffer->destroyMarkedObjects();
    gfx::sampler->destroyMarkedObjects();
    gfx::texture2D->destroyMarkedObjects();
    gfx::graphicsPSO->destroyMarkedObjects();
    
    // Reset render pass
    currentRenderPass = nullptr;
    if(currentRenderCmdEncoder != nullptr)
    {
        rgDelete(currentRenderCmdEncoder);
        currentRenderCmdEncoder = nullptr;
    }
    
    // TODO: reset unused frame allocator
}

rgInt getFrameIndex()
{
    return (g_FrameIndex != -1) ? g_FrameIndex : 0;
}

// TODO: rename -> getCompletedFrameIndex
rgInt getFinishedFrameIndex()
{
    rgInt completedFrameIndex = g_FrameIndex - RG_MAX_FRAMES_IN_FLIGHT + 1;
    completedFrameIndex = completedFrameIndex < 0 ? (RG_MAX_FRAMES_IN_FLIGHT + completedFrameIndex) : completedFrameIndex;

    checkerWaitTillFrameCompleted(completedFrameIndex);

    return completedFrameIndex;
}

GfxTexture2D* getCurrentRenderTargetColorBuffer()
{
    return gfx::renderTarget[g_FrameIndex];
}

GfxTexture2D* getRenderTargetDepthBuffer()
{
    return gfx::depthStencilBuffer;
}

GfxFrameAllocator* getFrameAllocator()
{
    return gfx::frameAllocators[g_FrameIndex];
}

GfxRenderCmdEncoder* setRenderPass(GfxRenderPass* renderPass, char const* tag)
{
    if(currentRenderPass != renderPass)
    {
        if(currentRenderPass != nullptr)
        {
            // end current render cmd list
            if(!currentRenderCmdEncoder->hasEnded)
            {
                currentRenderCmdEncoder->end();
            }
            rgDelete(currentRenderCmdEncoder);
        }
        // begin new cmd list
        currentRenderCmdEncoder = rgNew(GfxRenderCmdEncoder);
        currentRenderCmdEncoder->begin(tag, renderPass);

        currentRenderPass = renderPass;
    }
    else
    {
        rgAssert(!"Wrong usage");
        currentRenderCmdEncoder->pushDebugTag(tag);
    }
    
    return currentRenderCmdEncoder;
}

GfxBlitCmdEncoder* setBlitPass(char const* tag)
{
    if(currentBlitCmdEncoder != nullptr)
    {
        if(!currentBlitCmdEncoder->hasEnded)
        {
            currentBlitCmdEncoder->end();
        }
        
        rgDelete(currentBlitCmdEncoder);
    }
    
    currentBlitCmdEncoder = rgNew(GfxBlitCmdEncoder);
    currentBlitCmdEncoder->begin();
    currentBlitCmdEncoder->pushDebugTag(tag);
    
    return currentBlitCmdEncoder;
}

// --------------------

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, SimpleInstanceParams* instanceParams)
{
    rgUInt listSize = (rgUInt)quadList->size();
    for(rgUInt i = 0; i < listSize; ++i)
    {
        TexturedQuad& t = (*quadList)[i];
        
        SimpleVertexFormat v[4];
        // 0 - 1
        // 3 - 2
        v[0].pos[0] = t.pos.x;
        v[0].pos[1] = t.pos.y;
        v[0].pos[2] = t.pos.z;
        v[0].texcoord[0] = t.uv.uvTopLeft[0];
        v[0].texcoord[1] = t.uv.uvTopLeft[1];
        v[0].color[0] = 1.0f;
        v[0].color[1] = 1.0f;
        v[0].color[2] = 1.0f;
        v[0].color[3] = 1.0f;
        
        v[1].pos[0] = t.pos.x + t.size.x;
        v[1].pos[1] = t.pos.y;
        v[1].pos[2] = t.pos.z;
        v[1].texcoord[0] = t.uv.uvBottomRight[0];
        v[1].texcoord[1] = t.uv.uvTopLeft[1];
        v[1].color[0] = 1.0f;
        v[1].color[1] = 1.0f;
        v[1].color[2] = 1.0f;
        v[1].color[3] = 1.0f;
        
        v[2].pos[0] = t.pos.x + t.size.x;
        v[2].pos[1] = t.pos.y + t.size.y;
        v[2].pos[2] = t.pos.z;
        v[2].texcoord[0] = t.uv.uvBottomRight[0];
        v[2].texcoord[1] = t.uv.uvBottomRight[1];
        v[2].color[0] = 1.0f;
        v[2].color[1] = 1.0f;
        v[2].color[2] = 1.0f;
        v[2].color[3] = 1.0f;
        
        v[3].pos[0] = t.pos.x;
        v[3].pos[1] = t.pos.y + t.size.y;
        v[3].pos[2] = t.pos.z;
        v[3].texcoord[0] = t.uv.uvTopLeft[0];
        v[3].texcoord[1] = t.uv.uvBottomRight[1];
        v[3].color[0] = 1.0f;
        v[3].color[1] = 1.0f;
        v[3].color[2] = 1.0f;
        v[3].color[3] = 1.0f;
        
        vertices->push_back(v[0]);
        vertices->push_back(v[3]);
        vertices->push_back(v[1]);
        
        vertices->push_back(v[1]);
        vertices->push_back(v[3]);
        vertices->push_back(v[2]);
        
        instanceParams->texParam[i][0] = t.texID;
    }
} 

RG_END_GFX_NAMESPACE
RG_END_RG_NAMESPACE
