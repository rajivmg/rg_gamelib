#include "gfx.h"
#include "textdraw.h"
#include "viewport.h"

struct Bee : TheApp
{
    TinyImageFormat colorAttachmentFormat;
    TinyImageFormat depthStencilAttachmentFormat;
    GfxTexture *depthStencil;
    
    GfxTexture *colorRT; // unused

    FontRef testFont;
    
    GfxTexture *flower, *ocean, *arrow;
    GfxGraphicsPSO *texturedQuadPSO;
    
    Bee()
    {
        colorAttachmentFormat = gfxGetBackbufferFormat();
        depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT_S8_UINT;
    }
    
    void initializeShaders()
    {
        GfxVertexInputDesc vertexDesc = {};
        vertexDesc.elementCount = 3;
        
        vertexDesc.elements[0].semanticName = "POSITION";
        vertexDesc.elements[0].semanticIndex = 0;
        vertexDesc.elements[0].offset = 0;
        vertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
        vertexDesc.elements[0].bufferIndex = 0;
        vertexDesc.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
        
        vertexDesc.elements[1].semanticName = "TEXCOORD";
        vertexDesc.elements[1].semanticIndex = 0;
        vertexDesc.elements[1].offset = 12;
        vertexDesc.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
        vertexDesc.elements[1].bufferIndex = 0;
        vertexDesc.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;
        
        vertexDesc.elements[2].semanticName = "COLOR";
        vertexDesc.elements[2].semanticIndex = 0;
        vertexDesc.elements[2].offset = 20;
        vertexDesc.elements[2].format = TinyImageFormat_R8G8B8A8_UNORM;
        vertexDesc.elements[2].bufferIndex = 0;
        vertexDesc.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;
        
        GfxShaderDesc textureQuadShaderDesc = {};
        textureQuadShaderDesc.shaderSrc = "simple2d.hlsl";
        textureQuadShaderDesc.vsEntrypoint = "vsSimple2d";
        textureQuadShaderDesc.fsEntrypoint = "fsSimple2d";
        
        GfxRenderStateDesc texturedQuadRenderStateDesc = {};
        texturedQuadRenderStateDesc.colorAttachments[0].pixelFormat = colorAttachmentFormat;
        texturedQuadRenderStateDesc.colorAttachments[0].blendingEnabled = true;
        texturedQuadRenderStateDesc.depthStencilAttachmentFormat = depthStencilAttachmentFormat;
        
        texturedQuadPSO = GfxGraphicsPSO::create("texturedQuad", &vertexDesc, &textureQuadShaderDesc, &texturedQuadRenderStateDesc);
    }
    
    void loadTextures()
    {
		ImageRef flowerTex = loadImage("flower.png");
		flower = GfxTexture::create("flower", GfxTextureDim_2D, flowerTex->width, flowerTex->height, flowerTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, flowerTex->slices);

		ImageRef oceanTex = loadImage("ocean_tile.png");
		ocean = GfxTexture::create("ocean", GfxTextureDim_2D, oceanTex->width, oceanTex->height, oceanTex->format, oceanTex->mipFlag, GfxTextureUsage_ShaderRead, oceanTex->slices);
         
		ImageRef arrowTex = loadImage("test_arrow.png");
		arrow = GfxTexture::create("arrow", GfxTextureDim_2D, arrowTex->width, arrowTex->height, arrowTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, arrowTex->slices);
    }
    
    void setup() override
    {
        colorRT = GfxTexture::create("colorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
            colorAttachmentFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
        depthStencil = GfxTexture::create("depthStencil", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
            depthStencilAttachmentFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
        
        initializeShaders();
        loadTextures();
        testFont = loadFont("fonts/inconsolata_26.fnt");
    }
    
    void updateAndDraw() override
    {
        //ImGui::ShowDemoWindow();
        
        /*
         
                    [] []
                  [] [] []
                [] [] [] []
            ||[] [] [] [] []
            ||      []  []
            ||           ||
            \?====/==\===||
            ||    (___) bee
            ||
            ||
            ||       ;
            ||      wax
         ========================================
         
         */

        TexturedQuads testQuads;
        pushTexturedQuad(&testQuads, SpriteLayer_1, defaultQuadUV, {100.0f, 75.0f, 200.f, 200.f}, 0xFFFFFFFF, {0, 0, 0, 0}, flower);
        pushTexturedQuad(&testQuads, SpriteLayer_0, defaultQuadUV, {110.0f, 85.0f, 200.f, 200.f}, 0xFF0000FF, {0, 0, 0, 0}, flower);
        pushTexturedQuad(&testQuads, SpriteLayer_0, defaultQuadUV, {120.0f, 95.0f, 200.f, 200.f}, 0x00FF00FF, {0, 0, 0, 0}, flower);
        
        char const* splashIntroText = "It is not our part to master all the tides of the world,\nbut to do what is in us for the succour of those years wherein we are set,\nuprooting the evil in the fields that we know,\nso that those who live after may have clean earth to till.\nWhat weather they shall have is not ours to rule.";
        
        pushText(&testQuads, 100, 500, testFont, 1.0f, "Hello from rg_gamelib");
        pushText(&testQuads, 300, 100, testFont, 1.0f, splashIntroText);

        Matrix4 worldProjMatrix = makeOrthographicProjectionMatrix(0, 16.0f, 9.0f, 0, 0.1f, 10.0f);
        static rgFloat2 worldCamP;
        ImGui::DragFloat2("worldCamP", worldCamP.v, 0.1f);
        Matrix4 worldViewMatrix = Matrix4::lookAt(Point3(worldCamP.x, worldCamP.y, 0), Point3(worldCamP.x, worldCamP.y, -1000.0f), Vector3(0, 1.0f, 0));
        TexturedQuads worldTexturedQuads;
        static f32 ang;
        ang += (float)theAppInput->deltaTime;
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, 1.0f, 1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, ocean);
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, -1.0f, 1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, ocean);
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, -1.0f, -1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, ocean);
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {7.0f, 5.0f, 1.0f, 1.0f}, 0x00FF00FF, {0, 0, ang, 0}, ocean);
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {9.0f, 5.0f, 1.0f, 1.0f}, 0xFF0000FF, {0.5f, 0.5f, ang, 0}, ocean);
        
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {9.0f, 5.0f, 1.0f, 1.0f}, 0xFF0000FF, {-0.5f, -0.5f, ang, 0}, ocean);
        
        {
            f32 s = sinf(ang);
            f32 c = cosf(ang);
            rgFloat2 b = {7.0f + s * 2.0f, 5.0f + c * 2.0f};
            pushTexturedLine(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {7.0f, 5.0f}, b, 0.1f, 0xFF000FF, ocean);
        }

        {
            rgFloat2 p = { 600.0f, 300.0f };
            rgFloat2 m = {(f32)theAppInput->mouse.x, (f32)theAppInput->mouse.y};
            f32 l = length(m - p);
            f32 repeatU = l / 16.0f;
            pushTexturedLine(&testQuads, SpriteLayer_1, repeatQuadUV({repeatU, 1.0f}), p, m, 16.0f, 0x331de2ff, arrow);
        }

        for(i32 x = -8; x < 8; ++x)
        {
            pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, { (f32)x, 0, 1.0f, 1.0f }, 0x00FF00FF, { 0, 0, 0, 0 }, flower);
        }

        //
        GfxRenderPass simple2dRenderPass = {};
        simple2dRenderPass.colorAttachments[0].texture = gfxGetBackbufferTexture();
        simple2dRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dRenderPass.colorAttachments[0].clearColor = { 0.4f, 0.4f, 0.4f, 1.0f };
        simple2dRenderPass.depthStencilAttachmentTexture = depthStencil;
        simple2dRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dRenderPass.clearDepth = 0.0f;
        
        GfxRenderCmdEncoder* simple2dRenderEncoder = gfxSetRenderPass("Simple2D Pass", &simple2dRenderPass);
        simple2dRenderEncoder->setGraphicsPSO(texturedQuadPSO);
        simple2dRenderEncoder->drawTexturedQuads(&testQuads, nullptr, nullptr);
        simple2dRenderEncoder->drawTexturedQuads(&worldTexturedQuads, &worldViewMatrix, &worldProjMatrix);
        simple2dRenderEncoder->end();
        //
    }
};

THE_APP_MAIN(Bee)
