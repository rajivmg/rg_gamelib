#include "gfx.h"
#include "textdraw.h"
#include "viewport.h"

struct Smygel : TheApp
{
    TinyImageFormat colorRTFormat;
    TinyImageFormat depthStencilFormat;
    GfxTexture* colorRT;
    GfxTexture* depthStencil;
    FontRef testFont;
    
    Viewport* viewport;
    
    rgBool isMapEditorOpen;
    
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
        
        ImageRef oceanTex = loadImage("ocean_tile.png");
        GfxTexture::create("ocean", GfxTextureDim_2D, oceanTex->width, oceanTex->height, oceanTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, oceanTex->slices);
    
        ImageRef arrowTex = loadImage("test_arrow.png");
        GfxTexture::create("arrow", GfxTextureDim_2D, arrowTex->width, arrowTex->height, arrowTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, arrowTex->slices);
    }
    
    void setup() override
    {
        char const* splashIntroText = "It is not our part to master all the tides of the world,\nbut to do what is in us for the succour of those years wherein we are set,\nuprooting the evil in the fields that we know,\nso that those who live after may have clean earth to till.\nWhat weather they shall have is not ours to rule.";
        
        colorRT = GfxTexture::create("colorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                     colorRTFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
        depthStencil = GfxTexture::create("depthStencil", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height,
                                          depthStencilFormat, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
        
        createPipelineStateObjects();
        //pushText(&theGameState->tempTextQuads, FontRef, 50, 100, 32.0f, splashIntroText);
        testFont = loadFont("fonts/inconsolata_26.fnt");
        
        viewport = rgNew(Viewport);
    }
    
    void updateAndDraw() override
    {
        ImGui::ShowDemoWindow();
        mapEditorUpdateAndDraw();
        viewport->tick();
        
        CameraParamsGPU* cameraParams = viewport->getCameraParamsGPU();
        GfxFrameResource cameraParamsBuffer = gfxGetFrameAllocator()->newBuffer("cameraParams", sizeof(CameraParamsGPU), cameraParams);

        TexturedQuads testQuads;
        pushTexturedQuad(&testQuads, SpriteLayer_1, defaultQuadUV, {100.0f, 75.0f, 200.f, 200.f}, 0xFFFFFFFF, {0, 0, 0, 0}, GfxTexture::find("flower"_rghash));
        pushTexturedQuad(&testQuads, SpriteLayer_0, defaultQuadUV, {110.0f, 85.0f, 200.f, 200.f}, 0xFF0000FF, {0, 0, 0, 0}, GfxTexture::find("flower"_rghash));
        pushTexturedQuad(&testQuads, SpriteLayer_0, defaultQuadUV, {120.0f, 95.0f, 200.f, 200.f}, 0x00FF00FF, {0, 0, 0, 0}, GfxTexture::find("flower"_rghash));
        
        char const* splashIntroText = "It is not our part to master all the tides of the world,\nbut to do what is in us for the succour of those years wherein we are set,\nuprooting the evil in the fields that we know,\nso that those who live after may have clean earth to till.\nWhat weather they shall have is not ours to rule.";
        
        pushText(&testQuads, 100, 500, testFont, 1.0f, "Hello from rg_gamelib");
        pushText(&testQuads, 300, 100, testFont, 1.0f, splashIntroText);

        Matrix4 worldProjMatrix = makeOrthographicProjectionMatrix(0, 16.0f, 9.0f, 0, 0.1f, 10.0f);
        static rgFloat2 worldCamP;
        ImGui::DragFloat2("worldCamP", worldCamP.v, 0.1f);
        Matrix4 worldViewMatrix = Matrix4::lookAt(Point3(worldCamP.x, worldCamP.y, 0), Point3(worldCamP.x, worldCamP.y, -1000.0f), Vector3(0, 1.0f, 0));
        TexturedQuads worldTexturedQuads;
        static rgFloat ang;
        ang += (float)theAppInput->deltaTime;
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, 1.0f, 1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, GfxTexture::find("ocean"_rghash));
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, -1.0f, 1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, GfxTexture::find("ocean"_rghash));
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {5.0f, 5.0f, -1.0f, -1.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, GfxTexture::find("ocean"_rghash));
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {7.0f, 5.0f, 1.0f, 1.0f}, 0x00FF00FF, {0, 0, ang, 0}, GfxTexture::find("ocean"_rghash));
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {9.0f, 5.0f, 1.0f, 1.0f}, 0xFF0000FF, {0.5f, 0.5f, ang, 0}, GfxTexture::find("ocean"_rghash));
        
        pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {9.0f, 5.0f, 1.0f, 1.0f}, 0xFF0000FF, {-0.5f, -0.5f, ang, 0}, GfxTexture::find("ocean"_rghash));
        
        {
            rgFloat s = sinf(ang);
            rgFloat c = cosf(ang);
            rgFloat2 b = {7.0f + s * 2.0f, 5.0f + c * 2.0f};
            pushTexturedLine(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, {7.0f, 5.0f}, b, 0.1f, 0xFF000FF, GfxTexture::find("ocean"_rghash));
        }

        {
            rgFloat2 p = { 600.0f, 300.0f };
            rgFloat2 m = {(rgFloat)theAppInput->mouse.x, (rgFloat)theAppInput->mouse.y};
            rgFloat l = length(m - p);
            rgFloat repeatU = l / 16.0f;
            pushTexturedLine(&testQuads, SpriteLayer_1, repeatQuadUV({repeatU, 1.0f}), p, m, 16.0f, 0x331de2ff, GfxTexture::find("arrow"_rghash));
        }

        for(rgInt x = -8; x < 8; ++x)
        {
            pushTexturedQuad(&worldTexturedQuads, SpriteLayer_0, defaultQuadUV, { (rgFloat)x, 0, 1.0f, 1.0f }, 0x00FF00FF, { 0, 0, 0, 0 }, GfxTexture::find("flower"_rghash));
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
        simple2dRenderEncoder->setGraphicsPSO(GfxGraphicsPSO::find("texturedQuad"_rghash));
        simple2dRenderEncoder->drawTexturedQuads(&testQuads, nullptr, nullptr);
        simple2dRenderEncoder->drawTexturedQuads(&worldTexturedQuads, &worldViewMatrix, &worldProjMatrix);
        simple2dRenderEncoder->end();
        //
    }
    
    void mapEditorUpdateAndDraw()
    {
        if(!ImGui::Begin("Map Editor", &isMapEditorOpen))
        {
            ImGui::End();
            return;
        }
        
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(2.0f / 7.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(2.0f / 7.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(2.0f / 7.0f, 0.8f, 0.8f));
        ImGui::Button("Save");
        ImGui::PopStyleColor(3);

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        static ImVec2 canvasPan = ImVec2(0.0f, 0.0f);
        ImVec2 canvasP0 = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImVec2 canvasP1 = ImVec2(canvasP0.x + canvasSize.x, canvasP0.y + canvasSize.y);
        
        drawList->AddRectFilled(canvasP0, canvasP1, IM_COL32(50, 50, 50, 255));
        drawList->AddRect(canvasP0, canvasP1, IM_COL32(255, 255, 255, 255));
        
        ImGui::InvisibleButton("mapcanvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool isHovered = ImGui::IsItemHovered();
        const bool isActive = ImGui::IsItemActive();
        const ImVec2 origin(canvasP0.x + canvasPan.x, canvasP0.y + canvasPan.y);
        const ImVec2 canvasMousePos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
        
        if(isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
        {
            canvasPan.x += io.MouseDelta.x;
            canvasPan.y += io.MouseDelta.y;
        }
        
        drawList->PushClipRect(canvasP0, canvasP1, true);
        const float GridStep = 16.0f;
        for(float x = fmodf(canvasPan.x, GridStep); x < canvasSize.x; x += GridStep)
        {
            drawList->AddLine(ImVec2(canvasP0.x + x, canvasP0.y), ImVec2(canvasP0.x + x, canvasP1.y), IM_COL32(200, 200, 200, 40));
        }
        for(float y = fmodf(canvasPan.y, GridStep); y < canvasSize.y; y += GridStep)
        {
            drawList->AddLine(ImVec2(canvasP0.x, canvasP0.y + y), ImVec2(canvasP1.x, canvasP0.y + y), IM_COL32(200, 200, 200, 40));
        }
        //for(float x = fmodf(canvasPan.x, GridStep); x < canvasSize.x; x += GridStep)
        //{
        //    for(float y = fmodf(canvasPan.y, GridStep); y < canvasSize.y; y += GridStep)
        //    {
        //        drawList->AddCircleFilled(ImVec2(canvasP0.x + x, canvasP0.y + y), 3, IM_COL32(250, 160, 170, 255));
        //    }
        //}
        drawList->PopClipRect();
        
        ImGui::End();
    }
};

THE_APP_MAIN(Smygel)
