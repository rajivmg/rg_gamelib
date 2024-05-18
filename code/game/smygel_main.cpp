#include "gfx.h"
#include "textdraw.h"

struct Smygel : TheApp
{
    TinyImageFormat colorRTFormat;
    TinyImageFormat depthStencilFormat;
    GfxTexture* colorRT;
    GfxTexture* depthStencil;
    FontRef testFont;
    
    Smygel()
    : depthStencilFormat(TinyImageFormat_D32_SFLOAT_S8_UINT)
    {
        colorRTFormat = gfxGetBackbufferFormat();
    }
    
    void createPipelineStateObjects()
    {
        GfxVertexInputDesc vfTexturedQuad = {};
        vfTexturedQuad.elementCount = 3;
        
        vfTexturedQuad.elements[0].semanticName = "POSITION";
        vfTexturedQuad.elements[0].semanticIndex = 0;
        vfTexturedQuad.elements[0].offset = 0;
        vfTexturedQuad.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
        vfTexturedQuad.elements[0].bufferIndex = 0;
        vfTexturedQuad.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
        
        vfTexturedQuad.elements[1].semanticName = "TEXCOORD";
        vfTexturedQuad.elements[1].semanticIndex = 0;
        vfTexturedQuad.elements[1].offset = 12;
        vfTexturedQuad.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
        vfTexturedQuad.elements[1].bufferIndex = 0;
        vfTexturedQuad.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;

        
        vfTexturedQuad.elements[2].semanticName = "COLOR";
        vfTexturedQuad.elements[2].semanticIndex = 0;
        vfTexturedQuad.elements[2].offset = 20;
        vfTexturedQuad.elements[2].format = TinyImageFormat_R8G8B8A8_UNORM;
        vfTexturedQuad.elements[2].bufferIndex = 0;
        vfTexturedQuad.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;
        
        GfxShaderDesc textureQuadShaderDesc = {};
        textureQuadShaderDesc.shaderSrc = "simple2d.hlsl";
        textureQuadShaderDesc.vsEntrypoint = "vsSimple2d";
        textureQuadShaderDesc.fsEntrypoint = "fsSimple2d";
        textureQuadShaderDesc.defines = "VALDIGT_SMART";
        
        GfxRenderStateDesc texturedQuadRenderStateDesc = {};
        texturedQuadRenderStateDesc.colorAttachments[0].pixelFormat = colorRTFormat;
        texturedQuadRenderStateDesc.colorAttachments[0].blendingEnabled = true;
        texturedQuadRenderStateDesc.depthStencilAttachmentFormat = depthStencilFormat;
        
        GfxGraphicsPSO::create("texturedQuad", &vfTexturedQuad, &textureQuadShaderDesc, &texturedQuadRenderStateDesc);
        
        // setup test texture
        ImageRef flowerTex = loadImage("flower.png");
        GfxTexture::create("flower", GfxTextureDim_2D, flowerTex->width, flowerTex->height, flowerTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, flowerTex->slices);
    }
    
    void setup() override
    {
        char const* splashIntroText = "It is not our part to master all the tides of the world, but to do what is in us for the succour of those years wherein we are set, uprooting the evil in the fields that we know, so that those who live after may have clean earth to till. What weather they shall have is not ours to rule.";
        
        colorRT = GfxTexture::create("colorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                     colorRTFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
        depthStencil = GfxTexture::create("depthStencil", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                          depthStencilFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
        
        createPipelineStateObjects();
        //pushText(&theGameState->tempTextQuads, FontRef, 50, 100, 32.0f, splashIntroText);
        testFont = loadFont("fonts/inconsolata_26.fnt");
    }
    
    void updateAndDraw() override
    {
        ImGui::Begin("Hello");
        ImGui::End();
        
        TexturedQuads testQuads;
        pushTexturedQuad(&testQuads, defaultQuadUV, {100.0f, 100.0f, 200.f, 200.f}, 0xFFFFFFFF, {0, 0, 0, 0}, GfxTexture::find("flower"_rh));
        
        pushText(&testQuads, 100, 500, testFont, 1.0f, "Hello from rg_gamelib");
        //
        GfxRenderPass simple2dRenderPass = {};
        simple2dRenderPass.colorAttachments[0].texture = gfxGetBackbufferTexture();
        simple2dRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dRenderPass.colorAttachments[0].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        simple2dRenderPass.depthStencilAttachmentTexture = depthStencil;
        simple2dRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dRenderPass.clearDepth = 1.0f;
        
        GfxRenderCmdEncoder* simple2dRenderEncoder = gfxSetRenderPass("Simple2D Pass", &simple2dRenderPass);
        simple2dRenderEncoder->setGraphicsPSO(GfxGraphicsPSO::find("texturedQuad"_rh));
        simple2dRenderEncoder->drawTexturedQuads(&testQuads);
        simple2dRenderEncoder->end();
        //
    }
};

//THE_APP_MAIN(Smygel)
