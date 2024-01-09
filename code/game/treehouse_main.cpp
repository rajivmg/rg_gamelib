#include "gfx.h"

struct Treehouse : TheApp
{
    TinyImageFormat colorRTFormat;
    TinyImageFormat depthStencilFormat;
    GfxTexture* colorRT;
    GfxTexture* depthStencil;
    
    Treehouse()
    : colorRTFormat(TinyImageFormat_R16G16B16A16_SFLOAT)
    , depthStencilFormat(TinyImageFormat_D32_SFLOAT_S8_UINT)
    {
    }
    
    void createPipelineStateObjects()
    {
        GfxVertexInputDesc pos3fTexCoord2fColor4u = {};
    }
    
    void setup() override
    {
        colorRT = GfxTexture::create("colorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                     colorRTFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
        depthStencil = GfxTexture::create("depthStencil", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                          depthStencilFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
    }
    
    void updateAndDraw() override
    {
        ImGui::Begin("Hello");
        ImGui::End();
    }
};

//THE_APP_MAIN(Treehouse)
