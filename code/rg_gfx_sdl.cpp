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

    gfxGetRenderCmdList()->draw();

    //rg::GfxTexture2DPtr t = gfxCtx()->sdl.tTex;

    //SDL_RenderCopy(sdlRndr(), t->sdlTexture, NULL, NULL);

    SDL_RenderPresent(sdlRndr());

    return 0;
}

GfxTexture2DPtr gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage)
{
    Texture* tex = texture.get();
    return gfxNewTexture2D(tex->buf, tex->name, tex->width, tex->height, tex->format, usage);
}

GfxTexture2DPtr gfxNewTexture2D(void* buf, char const* name, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage)
{
    rgAssert(buf != NULL);

    SDL_Texture* sdlTexture = SDL_CreateTexture(sdlRndr(), toSDLPixelFormat(format), toSDLResourceUsage(usage), width, height);
    
    SDL_Rect rect = { 0, 0, (rgInt)width, (rgInt)height };
    SDL_UpdateTexture(sdlTexture, &rect, buf, width * TinyImageFormat_ChannelCount(format) * 1);

    GfxTexture2DPtr t2dPtr = eastl::shared_ptr<GfxTexture2D>(rgNew(GfxTexture2D), gfxDeleteTexture2D);
    t2dPtr->width = width;
    t2dPtr->height = height;
    t2dPtr->pixelFormat = format;
    t2dPtr->sdlTexture = sdlTexture;
    strcpy(t2dPtr->name, name);

    return t2dPtr;
}

void gfxDeleteTexture2D(GfxTexture2D* t2d)
{
    SDL_DestroyTexture(t2d->sdlTexture);
    rgDelete(t2d);
}

void gfxHandleRenderCmdTexturedQuad(void const* _cmd)
{
    RenderCmdTexturedQuad* cmd = (RenderCmdTexturedQuad*)_cmd;
    SDL_RenderCopy(sdlRndr(),cmd->texture->sdlTexture, NULL, NULL);
}

RG_END_NAMESPACE
#endif