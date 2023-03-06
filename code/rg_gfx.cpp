//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_NAMESPACE

// --- Game Graphics APIs
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };

rgInt gfxCommonInit()
{
    GfxCtx* ctx = gfxCtx();

    ctx->graphicCmdLists[0] = rgNew(RenderCmdList)("graphic cmdlist 0");
    ctx->graphicCmdLists[1] = rgNew(RenderCmdList)("graphic cmdlist 1");
    
    ctx->orthographicMatrix = Matrix4::orthographic(0.0f, 720.0f, 720.0f, 0, 0.0f, 1000.0f);

    return 0;
}

GfxTexture2DPtr gfxGetTexture2DPtr(rgCRC32 id)
{
    GfxCtx::HashMapCrc32vsGfxTexture2D::iterator itr = gfxCtx()->textures2D.find(id);
    if(itr == gfxCtx()->textures2D.end())
    {
        rgLogWarn("Could not find texture2d with id %d", id);
        return nullptr;
    }
    return itr->second.get();
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

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices)
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
    strcpy(tptr->name, "[NONAME]");
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

void immTexturedQuad2(Texture* texture, QuadUV* quad, rgFloat x, rgFloat y, rgFloat orientationRad, rgFloat scaleX, rgFloat scaleY, rgFloat offsetX, rgFloat offsetY)
{
    
}

RG_END_NAMESPACE
