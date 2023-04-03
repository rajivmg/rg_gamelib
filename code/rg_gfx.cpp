//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_NAMESPACE

// --- Game Graphics APIs
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };

Matrix4 makeOrthoProjection(rgFloat left, rgFloat right, rgFloat bottom, rgFloat top, rgFloat near, rgFloat far)
{
    rgFloat length = 1.0f / (right - left);
    rgFloat height = 1.0f / (top - bottom);
    rgFloat depth  = 1.0f / (far - near);
    
    return Matrix4(Vector4(2.0f * length, 0, 0, 0),
                   Vector4(0, 2.0f * height, 0, 0),
                   Vector4(0, 0, depth, 0),
                   Vector4(-((right + left) * length), -((top +bottom) * height), -near * depth, 1.0f));
}

rgInt gfxCommonInit()
{
    GfxCtx* ctx = gfxCtx();

    ctx->graphicCmdLists[0] = rgNew(RenderCmdList)("graphic cmdlist 0");
    ctx->graphicCmdLists[1] = rgNew(RenderCmdList)("graphic cmdlist 1");
    
    ctx->orthographicMatrix = Matrix4::orthographic(0.0f, 720.0f, 720.0f, 0, 0.1f, 1000.0f);
    
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

GfxTexture2DPtr gfxGetTexture2DPtr(GfxTexture2DHandle handle)
{
    rgAssert(handle < gfxCtx()->textures2D.size());
    
    GfxTexture2DPtr ptr = nullptr;
    ptr = gfxCtx()->textures2D[handle].get();
    rgAssert(ptr != nullptr);
    
    return ptr;
}

static GfxTexture2DHandle getFreeTextures2DHandle()
{
    GfxTexture2DHandle texHandle = kInvalidHandle;
    
    if(!gfxCtx()->textures2DFreeHandles.empty())
    {
        texHandle = gfxCtx()->textures2DFreeHandles.back();
        gfxCtx()->textures2DFreeHandles.pop_back();
    }
    else
    {
        rgAssert(gfxCtx()->textures2D.size() <= UINT32_MAX);
        texHandle = (GfxTexture2DHandle)gfxCtx()->textures2D.size();
        gfxCtx()->textures2D.resize(texHandle + 1);
    }
    
    rgAssert(texHandle != kInvalidHandle);
    return texHandle;
}

GfxTexture2DHandle gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage)
{
    Texture* tex = texture.get();
    rgAssert(tex != nullptr);
    
    GfxTexture2DHandle texHandle = getFreeTextures2DHandle();
    GfxTexture2DRef t2dRef = creatorGfxTexture2D(texHandle, tex->buf, tex->width, tex->height, tex->format, usage, tex->name);
    gfxCtx()->textures2D[texHandle] = t2dRef;
    return texHandle;
}

GfxTexture2DHandle gfxNewTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage, char const* name)
{
    GfxTexture2DHandle texHandle = getFreeTextures2DHandle();
    GfxTexture2DRef t2dRef = creatorGfxTexture2D(texHandle, buf, width, height, format, usage, name);
    gfxCtx()->textures2D[texHandle] = t2dRef;
    return texHandle;
}

void gfxDeleteTexture2D(GfxTexture2DHandle handle)
{
    rgAssert(handle < gfxCtx()->textures2D.size());
    
    gfxCtx()->textures2D[handle] = GfxTexture2DRef();
    gfxCtx()->textures2DFreeHandles.push_back(handle);
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
