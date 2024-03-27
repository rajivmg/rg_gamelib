#include "gfx.h"

struct Smygel : TheApp
{
    TinyImageFormat colorRTFormat;
    TinyImageFormat depthStencilFormat;
    GfxTexture* colorRT;
    GfxTexture* depthStencil;
    
    Smygel()
    : depthStencilFormat(TinyImageFormat_D32_SFLOAT_S8_UINT)
    {
        colorRTFormat = gfxGetBackbufferFormat();
    }
    
    void createPipelineStateObjects()
    {
        GfxVertexInputDesc pos3fTexCoord2fColor4u = {};
    }
    
    void setup() override
    {
        char const* splashIntroText = "It is not our part to master all the tides of the world, but to do what is in us for the succour of those years wherein we are set, uprooting the evil in the fields that we know, so that those who live after may have clean earth to till. What weather they shall have is not ours to rule.";
        
        colorRT = GfxTexture::create("colorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                     colorRTFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
        depthStencil = GfxTexture::create("depthStencil", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                          depthStencilFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
        
        //pushText(&theGameState->tempTextQuads, FontRef, 50, 100, 32.0f, splashIntroText);
    }
    
    void updateAndDraw() override
    {
        ImGui::Begin("Hello");
        ImGui::End();
    }
};

THE_APP_MAIN(Smygel)
