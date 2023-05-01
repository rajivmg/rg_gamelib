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

GLenum toGLPixelFormat(TinyImageFormat format)
{
    GLenum glFormat = GL_INVALID_ENUM;
    switch(format)
    {
    case TinyImageFormat_R8G8B8A8_UNORM:
        glFormat = GL_RGBA8;
        break;
    case TinyImageFormat_B8G8R8A8_UNORM:
        glFormat = GL_BGRA8_EXT;
        break;
    case TinyImageFormat_D16_UNORM:
        glFormat = GL_DEPTH_COMPONENT16;
        break;
    default:
        rgAssert("toGLPixelFormat: Unknown pixel format encountered");
        break;
    }

    return glFormat;
}

rgInt init()
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

void destroy()
{

}

rgInt draw()
{
    glViewport(0, 0, 720, 720);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    getRenderCmdList()->draw();
    getRenderCmdList()->afterDraw();

    SDL_GL_SwapWindow(gfxCtx()->mainWindow);
    return 0;
}

// ===---===---===
// BUFFER
// ===---===---===
GfxBuffer* creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    return rgNew(GfxBuffer);
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
GfxTexture2D* creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
{
    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);

    rgUInt mipCount = 1;
    GLenum pixelFormat = toGLPixelFormat(format);
    glTextureStorage2D(texture, mipCount, pixelFormat, width, height);
    
    if(buf != NULL)
    {
        glTextureSubImage2D(texture, mipCount, 0, 0, width, height, pixelFormat, GL_UNSIGNED_BYTE, buf);
    }

    GfxTexture2D* tex2dPtr = rgNew(GfxTexture2D);
    name != nullptr ? strcpy(tex2dPtr->name, name) : strcpy(tex2dPtr->name, "[NoName]");
    tex2dPtr->width = width;
    tex2dPtr->height = height;
    tex2dPtr->pixelFormat = format;
    tex2dPtr->glTexture = texture;
    return tex2dPtr;
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
    GfxRenderPass* renderPass = &setRenderPass->renderPass;

}

void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTriangles(void const* cmd)
{

}

RG_END_NAMESPACE

#endif