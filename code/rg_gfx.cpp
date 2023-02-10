//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE

// --- Low-level Graphics Functions


// --- Game Graphics APIs

TextureQuad::TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx)
{
    uvTopLeft[0] = (rgFloat)xPx / refWidthPx;
    uvTopLeft[1] = (rgFloat)yPx / refHeightPx;
    
    uvBottomRight[0] = (xPx + widthPx) / (rgFloat)refWidthPx;
    uvBottomRight[1] = (yPx + heightPx) / (rgFloat)refHeightPx;
}

TextureQuad::TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture)
    : TextureQuad(xPx, yPx, widthPx, heightPx, refTexture->width, refTexture->height)
{

}

void immTexturedQuad2(Texture* texture, TextureQuad* quad, rgFloat x, rgFloat y, rgFloat orientationRad, rgFloat scaleX, rgFloat scaleY, rgFloat offsetX, rgFloat offsetY)
{
    
}

RG_END_NAMESPACE
