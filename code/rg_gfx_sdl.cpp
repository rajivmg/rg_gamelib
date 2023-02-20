#ifdef RG_SDL_RNDR
#include "rg_gfx.h"

//dispatch_fn *cmd::draw::dispatchFn = &rndr::Draw;
//dispatch_fn *cmd::draw_indexed::dispatchFn = &rndr::DrawIndexed;
//dispatch_fn *cmd::copy_const_buffer::dispatchFn = &rndr::CopyConstBuffer;
//dispatch_fn *cmd::draw_debug_lines::dispatchFn = &rndr::DrawDebugLines;

RG_BEGIN_NAMESPACE

static inline SDL_Renderer* sdlRndr()
{
    return gfxCtx()->sdl.renderer;
}

static inline SDL_PixelFormatEnum toSDLPixelFormat(TinyImageFormat f)
{
    switch(f)
    {
    case TinyImageFormat_R8G8B8A8_UNORM:
        return SDL_PIXELFORMAT_RGBA32;
    default:
        rgAssert(!"Format conversion not defined");
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

SDL_TextureAccess toSDLResourceUsage(GfxResourceUsage usage)
{
    switch(usage)
    {
        case GfxResourceUsage_Read:
            return SDL_TEXTUREACCESS_STATIC;
            break;

        case GfxResourceUsage_Write:
            return SDL_TEXTUREACCESS_STREAMING;
            break;

        case GfxResourceUsage_ReadWrite:
            return SDL_TEXTUREACCESS_TARGET;
            break;

        default:
            return SDL_TEXTUREACCESS_STATIC;
            break;
    }
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

    //rg::RenderCmdList gfxCtx = {};

    return 0;
}

rgInt gfxDraw()
{
    SDL_RenderClear(sdlRndr());



    SDL_RenderPresent(sdlRndr());

    return 0;
}

GfxTexture2DPtr gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage)
{
    Texture* tex = texture.get();

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRndr(), toSDLPixelFormat(tex->format), toSDLResourceUsage(usage), tex->width, tex->height);
    
    //GfxTexture2DPtr t2dPtr = eastl::make_shared<GfxTexture2D>(gfxDeleteTexture2D);
    GfxTexture2DPtr t2dPtr = eastl::shared_ptr<GfxTexture2D>(rgNew(GfxTexture2D), gfxDeleteTexture2D);
    t2dPtr->width = tex->width;
    t2dPtr->height = tex->height;
    t2dPtr->pixelFormat = tex->format;
    t2dPtr->sdlTexture = sdlTexture;
    strcpy(t2dPtr->name, tex->name);
    
    return t2dPtr;
}

void gfxDeleteTexture2D(GfxTexture2D* t2d)
{
    SDL_DestroyTexture(t2d->sdlTexture);
    rgDelete(t2d);
}

GfxTexture2D* gfxNewTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage)
{
    return nullptr;
}

void gfxHandleRenderCmdTexturedQuad(void const* cmd)
{

}

RG_END_NAMESPACE
#endif