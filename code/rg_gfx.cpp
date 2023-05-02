//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>
#include <EASTL/string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_NAMESPACE

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

RG_GFX_BEGIN_NAMESPACE

SDL_Window* mainWindow;
rgUInt frameNumber;

RenderCmdList* graphicCmdLists[RG_MAX_FRAMES_IN_FLIGHT];

GfxObjectRegistry<GfxRenderTarget>* registryRenderTarget;
GfxObjectRegistry<GfxTexture2D>* registryTexture2D;
GfxObjectRegistry<GfxBuffer>* registryBuffer;
GfxObjectRegistry<GfxGraphicsPSO>* registryGraphicsPSO;

GfxBindlessResourceManager<GfxTexture2D>* bindlessManagerTexture2D;

Matrix4 orthographicMatrix;
Matrix4 viewMatrix;

eastl::vector<GfxTexture2D*> debugTextureHandles; // test only

GfxTexture2D* renderTarget[RG_MAX_FRAMES_IN_FLIGHT];
GfxTexture2D* depthStencilBuffer;

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

rgInt preInit()
{
    gfx::registryRenderTarget = rgNew(GfxObjectRegistry<GfxRenderTarget>);
    gfx::registryTexture2D = rgNew(GfxObjectRegistry<GfxTexture2D>);
    gfx::registryBuffer = rgNew(GfxObjectRegistry<GfxBuffer>);
    gfx::registryGraphicsPSO = rgNew(GfxObjectRegistry<GfxGraphicsPSO>);

    gfx::bindlessManagerTexture2D = rgNew(GfxBindlessResourceManager<GfxTexture2D>);

    return 0;
}

rgInt initCommonStuff()
{
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        eastl::string tag = "graphicsCmdList" + i;
        gfx::graphicCmdLists[i] = rgNew(RenderCmdList)(tag.c_str());
    }

    gfx::orthographicMatrix = Matrix4::orthographic(0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height, 0, 0.1f, 1000.0f);
    gfx::viewMatrix = Matrix4::lookAt(Point3(0, 0, 0), Point3(0, 0, -1000.0f), Vector3(0, 1.0f, 0));
    
#ifdef RG_METAL_RNDR
    //Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 shiftZHalf = Matrix4::translation(Vector3(0.0f, 0.0f, 0.5f));
    //Matrix4 shiftZHalf = Matrix4::translation(Vector3(1.0f, -1.0f, -1000.0f / (0.1f - 1000.0f)));
    //Matrix4 scaleShiftZHalf = shiftZHalf * scaleZHalf;
    //ctx->orthographicMatrix = shiftZHalf * scaleZHalf * ctx->orthographicMatrix;
    ctx->orthographicMatrix = makeOrthoProjection(0.0f, 720.0f, 720.0f, 0.0f, 0.1f, 1000.0f);
#endif
    
    return 0;
}

RenderCmdList* getRenderCmdList()
{
    return graphicCmdLists[g_FrameIndex];
}
// --------------------

void allocAndFillRenderTargetStruct(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format, GfxRenderTarget** obj)
{
    *obj = rgNew(GfxRenderTarget);
    rgAssert(tag != nullptr);
    strncpy((*obj)->tag, tag, rgARRAY_COUNT(GfxRenderTarget::tag));
    (*obj)->width = width;
    (*obj)->height = height;
    (*obj)->format = format;
}

void deallocRenderTargetStruct(GfxRenderTarget* obj)
{
    rgDelete(obj);
}

GfxRenderTarget* createRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format)
{
    GfxRenderTarget* objPtr;
    allocAndFillRenderTargetStruct(tag, width, height, format, &objPtr);
    creatorGfxRenderTarget(tag, width, height, format, objPtr);
    gfx::registryRenderTarget->insert(rgCRC32(tag), objPtr);
    return objPtr;
}

GfxRenderTarget* findOrCreateRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format)
{
    GfxRenderTarget* objPtr = gfx::registryRenderTarget->find(rgCRC32(tag));
    objPtr = (objPtr == nullptr) ? createRenderTarget(tag, width, height, format) : objPtr;
    return objPtr;
}

GfxRenderTarget* findRenderTarget(rgHash tagHash)
{
    return gfx::registryRenderTarget->find(tagHash);
}

GfxRenderTarget* findRenderTarget(char const* tag)
{
    return findRenderTarget(rgCRC32(tag));
}

void destroyRenderTarget(rgHash tagHash)
{
    gfx::registryRenderTarget->markForRemove(tagHash);
}

void destroyRenderTarget(char const* tag)
{
    gfx::registryRenderTarget->markForRemove(rgCRC32(tag));
}

///

void allocAndFillTexture2DStruct(const char* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, GfxTexture2D** obj)
{
    *obj = rgNew(GfxTexture2D);
    rgAssert(tag != nullptr);
    strncpy((*obj)->tag, tag, rgARRAY_COUNT(GfxTexture2D::tag));
    (*obj)->width = width;
    (*obj)->height = height;
    (*obj)->format = format;
    (*obj)->usage = usage;
}

void deallocTexture2DStruct(GfxTexture2D* obj)
{
    rgDelete(obj);
}

GfxTexture2D* createTexture2D(char const* tag, TextureRef texture, GfxTextureUsage usage)
{
    return createTexture2D(tag, texture->buf, texture->width, texture->height, texture->format, usage);
}

GfxTexture2D* createTexture2D(const char* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage)
{
    GfxTexture2D* objPtr;
    allocAndFillTexture2DStruct(tag, buf, width, height, format, usage, &objPtr);
    creatorGfxTexture2D(tag, buf, width, height, format, usage, objPtr);
    gfx::registryTexture2D->insert(rgCRC32(tag), objPtr);
    return objPtr;
}

GfxTexture2D* findOrCreateTexture2D(const char* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage)
{
    GfxTexture2D* objPtr = gfx::registryTexture2D->find(rgCRC32(tag));
    objPtr = (objPtr == nullptr) ? createTexture2D(tag, buf, width, height, format, usage) : objPtr;
    return objPtr;
}

GfxTexture2D* findTexture2D(rgHash tagHash)
{
    return gfx::registryTexture2D->find(tagHash);
}

GfxTexture2D* findTexture2D(char const* tag)
{
    return findTexture2D(rgCRC32(tag));
}

void destroyTexture2D(rgHash tagHash)
{
    gfx::registryTexture2D->markForRemove(tagHash);
}

void destroyTexture2D(char const* tag)
{
    gfx::registryTexture2D->markForRemove(rgCRC32(tag));
}

///

void updateBuffer(rgHash tagHash, void* buf, rgU32 size, rgU32 offset)
{
    GfxBuffer* obj = findBuffer(tagHash);
    rgAssert(obj != nullptr);
    updaterGfxBuffer(buf, size, offset, obj);
}

void updateBuffer(char const* tag, void* buf, rgU32 size, rgU32 offset)
{
    updateBuffer(rgCRC32(tag), buf, size, offset);
}

void allocAndFillBufferStruct(const char* tag, void* buf, rgU32 size, GfxResourceUsage usage, GfxBuffer** obj)
{
    *obj = rgNew(GfxBuffer);
    rgAssert(tag != nullptr);
    strncpy((*obj)->tag, tag, rgARRAY_COUNT(GfxBuffer::tag));
    (*obj)->size = size;
    (*obj)->usage = usage;
    (*obj)->activeSlot = 0;
}

void deallocBufferStruct(GfxBuffer* obj)
{
    rgDelete(obj);
}

GfxBuffer* createBuffer(const char* tag, void* buf, rgU32 size, GfxResourceUsage usage)
{
    GfxBuffer* objPtr;
    allocAndFillBufferStruct(tag, buf, size, usage, &objPtr);
    creatorGfxBuffer(tag, buf, size, usage, objPtr);
    gfx::registryBuffer->insert(rgCRC32(tag), objPtr);
    return objPtr;
}

GfxBuffer* findOrCreateBuffer(const char* tag, void* buf, rgU32 size, GfxResourceUsage usage)
{
    GfxBuffer* objPtr = gfx::registryBuffer->find(rgCRC32(tag));
    objPtr = (objPtr == nullptr) ? createBuffer(tag, buf, size, usage) : objPtr;
    return objPtr;
}

GfxBuffer* findBuffer(rgHash tagHash)
{
    return gfx::registryBuffer->find(tagHash);
}

GfxBuffer* findBuffer(char const* tag)
{
    return findBuffer(rgCRC32(tag));
}

void destroyBuffer(rgHash tagHash)
{
    gfx::registryBuffer->markForRemove(tagHash);
}

void destroyBuffer(char const* tag)
{
    gfx::registryBuffer->markForRemove(rgCRC32(tag));
}

///

void allocAndFillGraphicsPSOStruct(const char* tag, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO** obj)
{
    *obj = rgNew(GfxGraphicsPSO);
    rgAssert(tag != nullptr);
    strncpy((*obj)->tag, tag, rgARRAY_COUNT(GfxGraphicsPSO::tag));
    (*obj)->renderState = *renderStateDesc;
}

void deallocGraphicsPSOStruct(GfxGraphicsPSO* obj)
{
    rgDelete(obj);
}

GfxGraphicsPSO* createGraphicsPSO(const char* tag, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* objPtr;
    allocAndFillGraphicsPSOStruct(tag, shaderDesc, renderStateDesc, &objPtr);
    creatorGfxGraphicsPSO(tag, shaderDesc, renderStateDesc, objPtr);
    gfx::registryGraphicsPSO->insert(rgCRC32(tag), objPtr);
    return objPtr;
}

GfxGraphicsPSO* findOrCreateGraphicsPSO(const char* tag, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    GfxGraphicsPSO* objPtr = gfx::registryGraphicsPSO->find(rgCRC32(tag));
    objPtr = (objPtr == nullptr) ? createGraphicsPSO(tag, shaderDesc, renderStateDesc) : objPtr;
    return objPtr;
}

GfxGraphicsPSO* findGraphicsPSO(rgHash tagHash)
{
    return gfx::registryGraphicsPSO->find(tagHash);
}

GfxGraphicsPSO* findGraphicsPSO(char const* tag)
{
    return findGraphicsPSO(rgCRC32(tag));
}

void destroyGraphicsPSO(rgHash tagHash)
{
    gfx::registryGraphicsPSO->markForRemove(tagHash);
}

void destroyGraphicsPSO(char const* tag)
{
    gfx::registryGraphicsPSO->markForRemove(rgCRC32(tag));
}

///

//GfxRenderTarget* gfxCreateRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format)
//GfxRenderTarget* gfxFindOrCreateRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format)
//GfxRenderTarget* gfxFindRenderTarget(rgHash tagHash)
//GfxRenderTarget* gfxFindRenderTarget(char const* tag)
//void gfxDestroyRenderTarget(rgHash tagHash)
//void gfxDestroyRenderTarget(char const* tag)

// ------

/*
TexturedQuad::TexturedQuad(QuadUV uv_, Vector4 posScale_, Vector4 offsetOrientation_)
    : uv(uv_)
    , posScale(posScale_)
    , offsetOrientation(offsetOrientation_)
{
}
*/

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, eastl::vector<SimpleInstanceParams>* instanceParams)
{
    rgUInt listSize = (rgUInt)quadList->size();
    for(rgUInt i = 0; i < listSize; ++i)
    {
        TexturedQuad& t = (*quadList)[i];
        
        SimpleVertexFormat v[4];
        // 0 - 1
        // 3 - 2
        v[0].pos[0] = t.posSize.x;
        v[0].pos[1] = t.posSize.y;
        v[0].texcoord[0] = t.uv.uvTopLeft[0];
        v[0].texcoord[1] = t.uv.uvTopLeft[1];
        v[0].color[0] = 1.0f;
        v[0].color[1] = 1.0f;
        v[0].color[2] = 1.0f;
        v[0].color[3] = 1.0f;
        
        v[1].pos[0] = t.posSize.x + t.posSize.z;
        v[1].pos[1] = t.posSize.y;
        v[1].texcoord[0] = t.uv.uvBottomRight[0];
        v[1].texcoord[1] = t.uv.uvTopLeft[1];
        v[1].color[0] = 1.0f;
        v[1].color[1] = 1.0f;
        v[1].color[2] = 1.0f;
        v[1].color[3] = 1.0f;
        
        v[2].pos[0] = t.posSize.x + t.posSize.z;
        v[2].pos[1] = t.posSize.y + t.posSize.w;
        v[2].texcoord[0] = t.uv.uvBottomRight[0];
        v[2].texcoord[1] = t.uv.uvBottomRight[1];
        v[2].color[0] = 1.0f;
        v[2].color[1] = 1.0f;
        v[2].color[2] = 1.0f;
        v[2].color[3] = 1.0f;
        
        v[3].pos[0] = t.posSize.x;
        v[3].pos[1] = t.posSize.y + t.posSize.w;
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
        
        SimpleInstanceParams instParam;
        instParam.texID = gfx::bindlessManagerTexture2D->getBindlessIndex(t.tex);
        instanceParams->push_back(instParam);
    }
}

RG_GFX_END_NAMESPACE
RG_END_NAMESPACE
