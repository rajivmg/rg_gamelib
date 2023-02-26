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
