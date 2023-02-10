#ifdef RG_SDL_RNDR
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE

static inline SDL_Renderer* sdlRndr()
{
    return gfxCtx()->sdl.renderer;
}

rgInt gfxInit()
{
    Uint32 flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE;
    SDL_Renderer* renderer = SDL_CreateRenderer(gfxCtx()->mainWindow, -1, flags);

    if(renderer)
    {
        gfxCtx()->sdl.renderer = renderer;
    }
    else
    {
        rgAssert(!"SDL Renderer creation failed");
    }

    return 0;
}

rgInt gfxDraw()
{
    SDL_RenderClear(sdlRndr());



    SDL_RenderPresent(sdlRndr());

    return 0;
}

RG_END_NAMESPACE
#endif