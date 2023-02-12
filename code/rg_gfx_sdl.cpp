#ifdef RG_SDL_RNDR
#include "rg_gfx.h"

//dispatch_fn *cmd::draw::DISPATCH_FUNCTION = &rndr::Draw;
//dispatch_fn *cmd::draw_indexed::DISPATCH_FUNCTION = &rndr::DrawIndexed;
//dispatch_fn *cmd::copy_const_buffer::DISPATCH_FUNCTION = &rndr::CopyConstBuffer;
//dispatch_fn *cmd::draw_debug_lines::DISPATCH_FUNCTION = &rndr::DrawDebugLines;

RG_BEGIN_NAMESPACE

static inline SDL_Renderer* sdlRndr()
{
    return gfxCtx()->sdl.renderer;
}

void RenderTexturedQuad(void const* cmd)
{

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

void gfxHandleRenderCmdTexturedQuad(void const* cmd)
{

}

RG_END_NAMESPACE
#endif