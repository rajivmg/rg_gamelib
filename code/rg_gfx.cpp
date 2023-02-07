//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE

// TODO: Add filename and line number, try to remove the need of fmt
void _LogImpl(char const* fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_TEST, SDL_LOG_PRIORITY_ERROR, fmt, argList);
    va_end(argList);
}

// --- Low-level Graphics Functions

GfxCtx* gfxCtx()
{
    return &g_GfxCtx;
}

// --- Game Graphics APIs

TextureQuad::TextureQuad(U32 xPx, U32 yPx, U32 widthPx, U32 heightPx, U32 refWidthPx, U32 refHeightPx)
{
    uvTopLeft[0] = xPx / refWidthPx;
    uvTopLeft[1] = yPx / refHeightPx;
    
    uvBottomRight[0] = (xPx + widthPx) / refWidthPx;
    uvBottomRight[1] = (yPx + heightPx) / refHeightPx;
}

TextureQuad::TextureQuad(U32 xPx, U32 yPx, U32 widthPx, U32 heightPx, Texture* refTexture)
    : TextureQuad(xPx, yPx, widthPx, heightPx, refTexture->width, refTexture->height)
{

}

void immTexturedQuad2(Texture* texture, TextureQuad* quad, R32 x, R32 y, R32 orientationRad, R32 scaleX, R32 scaleY, R32 offsetX, R32 offsetY)
{
    
}

RG_END_NAMESPACE
