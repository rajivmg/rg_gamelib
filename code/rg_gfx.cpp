//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_NAMESPACE

// --- Game Graphics APIs
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };

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

rgInt gfxCommonInit()
{
    GfxCtx* ctx = gfxCtx();

    ctx->graphicCmdLists[0] = rgNew(RenderCmdList)("graphic cmdlist 0");
    ctx->graphicCmdLists[1] = rgNew(RenderCmdList)("graphic cmdlist 1");
    ctx->graphicCmdLists[2] = rgNew(RenderCmdList)("graphic cmdlist 2");
    
    ctx->orthographicMatrix = Matrix4::orthographic(0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height, 0, 0.1f, 1000.0f);
    
#ifdef RG_METAL_RNDR
    //Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 scaleZHalf = Matrix4::scale(Vector3(1.0f, 1.0f, -0.5f));
    Matrix4 shiftZHalf = Matrix4::translation(Vector3(0.0f, 0.0f, 0.5f));
    //Matrix4 shiftZHalf = Matrix4::translation(Vector3(1.0f, -1.0f, -1000.0f / (0.1f - 1000.0f)));
    //Matrix4 scaleShiftZHalf = shiftZHalf * scaleZHalf;
    //ctx->orthographicMatrix = shiftZHalf * scaleZHalf * ctx->orthographicMatrix;
    ctx->orthographicMatrix = makeOrthoProjection(0.0f, 720.0f, 720.0f, 0.0f, 0.1f, 1000.0f);
#endif
    
    ctx->viewMatrix = Matrix4::lookAt(Point3(0, 0, 0), Point3(0, 0, -1000.0f), Vector3(0, 1.0f, 0));
    return 0;
}

HGfxBuffer gfxNewBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    HGfxBuffer bufferHandle = gfxCtx()->buffersManager.getFreeHandle();
    GfxBuffer* bufferPtr = creatorGfxBuffer(data, size, usage);
    gfxCtx()->buffersManager.setResourcePtrForHandle(bufferHandle, bufferPtr);
    return bufferHandle;
}

void gfxUpdateBuffer(HGfxBuffer handle, void* data, rgSize size, rgU32 offset)
{
    GfxBuffer* buffer = gfxCtx()->buffersManager.getPtr(handle);
    updaterGfxBuffer(buffer, data, size, offset);
}

void gfxDeleteBuffer(HGfxBuffer handle)
{
    deleterGfxBuffer(gfxBufferPtr(handle));
    gfxCtx()->buffersManager.releaseHandle(handle);
}

GfxBuffer* gfxBufferPtr(HGfxBuffer bufferHandle)
{
    return gfxCtx()->buffersManager.getPtr(bufferHandle);
}

HGfxTexture2D gfxNewTexture2D(TexturePtr texture, GfxTextureUsage usage)
{
    Texture* tex = texture.get();
    rgAssert(tex != nullptr);
    
    HGfxTexture2D tex2dHandle = gfxCtx()->texture2dManager.getFreeHandle();
    GfxTexture2D* tex2dPtr = creatorGfxTexture2D(tex->buf, tex->width, tex->height, tex->format, usage, tex->name);
    tex2dPtr->texID = tex2dHandle;
    gfxCtx()->texture2dManager.setResourcePtrForHandle(tex2dHandle, tex2dPtr);
    return tex2dHandle;
}

HGfxTexture2D gfxNewTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
{
    HGfxTexture2D tex2dHandle = gfxCtx()->texture2dManager.getFreeHandle();
    GfxTexture2D* tex2dPtr = creatorGfxTexture2D(buf, width, height, format, usage, name);
    tex2dPtr->texID = tex2dHandle;
    gfxCtx()->texture2dManager.setResourcePtrForHandle(tex2dHandle, tex2dPtr);
    return tex2dHandle;
}

void gfxDeleteTexture2D(HGfxTexture2D handle)
{
    deleterGfxTexture2D(gfxTexture2DPtr(handle));
    gfxCtx()->texture2dManager.releaseHandle(handle);
}

GfxTexture2D* gfxTexture2DPtr(HGfxTexture2D texture2dHandle)
{
    return gfxCtx()->texture2dManager.getPtr(texture2dHandle);
}

HGfxGraphicsPSO gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    HGfxGraphicsPSO psoHandle = gfxCtx()->graphicsPSOManager.getFreeHandle();
    GfxGraphicsPSO* psoPtr = creatorGfxGraphicsPSO(shaderDesc, renderStateDesc);
    gfxCtx()->graphicsPSOManager.setResourcePtrForHandle(psoHandle, psoPtr);
    return psoHandle;
}

void gfxDeleleGraphicsPSO(HGfxGraphicsPSO handle)
{
    deleterGfxGraphicsPSO(gfxGraphicsPSOPtr(handle));
    gfxCtx()->graphicsPSOManager.releaseHandle(handle);
}

GfxGraphicsPSO* gfxGraphicsPSOPtr(HGfxGraphicsPSO handle)
{
    return gfxCtx()->graphicsPSOManager.getPtr(handle);
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
        instParam.texID = t.texID;
        instanceParams->push_back(instParam);
    }
}

TexturePtr loadTexture(char const* filename)
{
    rgInt width, height, texChnl;
    unsigned char* texData = stbi_load(filename, &width, &height, &texChnl, 4);
    
    if(texData == NULL)
    {
        return nullptr;
    }

    //TexturePtr tptr = eastl::make_shared<Texture>(unloadTexture);
    TexturePtr tptr = eastl::shared_ptr<Texture>(rgNew(Texture), unloadTexture);
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

RG_END_NAMESPACE
