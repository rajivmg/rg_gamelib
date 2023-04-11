#if defined(RG_OPENGL_RNDR)
#include "rg_gfx.h"
#include <GL/glew.h>

RG_BEGIN_NAMESPACE

#if 1
static inline GfxCtx::GL* gl()
{
    return &g_GfxCtx->gl;
}
#else
#define gl() (&g_GfxCtx->gl)
#endif

rgInt gfxInit()
{
    gl()->context = SDL_GL_CreateContext(gfxCtx()->mainWindow);
    rgAssert(gl()->context);

    GLenum glewResult = glewInit();
    if(glewResult != GLEW_OK)
    {
        rgLogError("glewInit() failed");
    }
    
    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);

    return 0;
}

void gfxDestroy()
{

}

rgInt gfxDraw()
{
    glViewport(0, 0, 720, 720);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gfxGetRenderCmdList()->draw();
    gfxGetRenderCmdList()->afterDraw();

    SDL_GL_SwapWindow(gfxCtx()->mainWindow);
    return 0;
}

// ===---===---===
// BUFFER
// ===---===---===
GfxBufferRef creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    return GfxBufferRef(rgNew(GfxBuffer), deleterGfxBuffer);
}

void updaterGfxBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset)
{

}

void deleterGfxBuffer(GfxBuffer* buffer)
{

}

// ===---===---===
// TEXTURE
// ===---===---===
GfxTexture2DRef creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
{
    return GfxTexture2DRef(rgNew(GfxTexture2D), deleterGfxTexture2D);
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{

}

// ===---===---===
// GRAPHICS PSO
// ===---===---===
GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    return nullptr;
}

void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso)
{

}

// ===---===---===
// RenderCmd HANDLERS
// ===---===---===
void gfxHandleRenderCmd_SetRenderPass(void const* cmd)
{
    RenderCmd_SetRenderPass* setRenderPass = (RenderCmd_SetRenderPass*)cmd;
    
}

void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTriangles(void const* cmd)
{

}

RG_END_NAMESPACE

#endif