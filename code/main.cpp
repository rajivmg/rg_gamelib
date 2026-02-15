#include "core.h"

#include "gfx.h"
#include "viewport.h"
#include "rg_physic.h"

#include "backends/imgui_impl_sdl2.h"

#include "box2d/box2d.h"

#include <EASTL/vector.h>
#include "game/game.h"
#include "game/skybox.h"
#include "game/textdraw.h"

#include "shaders/shaderinterop_common.h"

// TODO:
// 2. Add rgLogDebug() rgLogError()
// 3. Then use the suitable rgLogXXX() version in VKDbgReportCallback Function based on msgType 
// 4. Integrate EASTL
//

GameState* g_GameState;
PhysicSystem* g_PhysicSystem;
Viewport* g_Viewport;

eastl::vector<GfxTexture*> debugTextureHandles;

FontRef inconFont;

GfxGraphicsPSO* simple2DPSO;
GfxGraphicsPSO* skyboxPSO;
GfxComputePSO*  tonemapGenerateHistogramPSO;
GfxComputePSO*  tonemapClearOutputLuminanceHistogramPSO;
GfxComputePSO*  tonemapComputeAvgLuminancePSO;
GfxComputePSO*  tonemapReinhardPSO;
GfxComputePSO*  compositePSO;
GfxGraphicsPSO* principledBrdfPSO;
GfxGraphicsPSO* gridPSO;

GfxTexture* sangiuseppeBridgeCubeTex;
GfxTexture* sangiuseppeBridgeCubeIrradianceTex;
GfxTexture* japaneseStoneWallDiff1kTex;

GfxBuffer* skyboxVertexBuffer;
GfxBuffer* outputLuminanceHistogramBuffer;

i32 setup()
{
    g_GameState = rgNew(GameState);

    ImageRef tinyTexture = loadImage("tiny.tga");
    GfxTexture::create("tiny", GfxTextureDim_2D, tinyTexture->width, tinyTexture->height, tinyTexture->format, GfxTextureMipFlag_1Mip, GfxTextureUsage_ShaderRead, tinyTexture->slices);
    //gfx::texture->destroy(rgCRC32("tiny"));
    // TODO: don't call destoy on first frame  gfx::onFrameBegin
    
    for(i32 i = 1; i <= 16; ++i)
    {
        char path[256];
        snprintf(path, 256, "debugTextures/textureSlice%d.png", i);
        ImageRef t = loadImage(path);
        GfxTexture* t2d = GfxTexture::create(path, GfxTextureDim_2D, t->width, t->height, t->format, GfxTextureMipFlag_1Mip, GfxTextureUsage_ShaderRead, t->slices);
        debugTextureHandles.push_back(t2d);
    }

    ImageRef flowerTex = loadImage("flower.png");
    g_GameState->flowerTexture = GfxTexture::create("flower", GfxTextureDim_2D, flowerTex->width, flowerTex->height, flowerTex->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, flowerTex->slices);
    
    //gfxDestroyBuffer("ocean_tile");
    g_GameState->shaderballModel = loadModel("shaderball_test1.xml");
    
    GfxVertexInputDesc vertexDesc = {};
    vertexDesc.elementCount = 3;
    vertexDesc.elements[0].semanticName = "POSITION";
    vertexDesc.elements[0].semanticIndex = 0;
    vertexDesc.elements[0].offset = 0;
    vertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexDesc.elements[0].bufferIndex = 0; // TODO: rename to slot
    vertexDesc.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexDesc.elements[1].semanticName = "TEXCOORD";
    vertexDesc.elements[1].semanticIndex = 0;
    vertexDesc.elements[1].offset = 12;
    vertexDesc.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
    vertexDesc.elements[1].bufferIndex = 0; // TODO: Automate this.. MTL: 30 - bufferIndex = vb bindpoint, to maintain compat with DX12 way
    vertexDesc.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexDesc.elements[2].semanticName = "COLOR";
    vertexDesc.elements[2].semanticIndex = 0;
    vertexDesc.elements[2].offset = 20;
    vertexDesc.elements[2].format = TinyImageFormat_R8G8B8A8_UNORM;
    vertexDesc.elements[2].bufferIndex = 0;
    vertexDesc.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrc = "simple2d.hlsl";
    simple2dShaderDesc.vsEntrypoint = "vsSimple2d";
    simple2dShaderDesc.fsEntrypoint = "fsSimple2d";
    simple2dShaderDesc.defines = "RIGHT";
    
    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_R8G8B8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    //simple2dRenderStateDesc.triangleFillMode = GfxTriangleFillMode_Lines;
    
    simple2DPSO = GfxGraphicsPSO::create("simple2d", &vertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    
    //
    GfxVertexInputDesc vertexPosTexCoordNormal = {};
    vertexPosTexCoordNormal.elementCount = 3;
    vertexPosTexCoordNormal.elements[0].semanticName = "POSITION";
    vertexPosTexCoordNormal.elements[0].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[0].offset = 0;
    vertexPosTexCoordNormal.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexPosTexCoordNormal.elements[0].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexPosTexCoordNormal.elements[1].semanticName = "TEXCOORD";
    vertexPosTexCoordNormal.elements[1].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[1].offset = 12;
    vertexPosTexCoordNormal.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
    vertexPosTexCoordNormal.elements[1].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexPosTexCoordNormal.elements[2].semanticName = "NORMAL";
    vertexPosTexCoordNormal.elements[2].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[2].offset = 20;
    vertexPosTexCoordNormal.elements[2].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexPosTexCoordNormal.elements[2].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc principledBrdfShaderDesc = {};
    principledBrdfShaderDesc.shaderSrc = "pbr.hlsl";
    principledBrdfShaderDesc.vsEntrypoint = "vsPbr";
    principledBrdfShaderDesc.fsEntrypoint = "fsPbr";
    principledBrdfShaderDesc.defines = "HAS_VERTEX_NORMAL";
    
    GfxRenderStateDesc world3dRenderState = {};
    world3dRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
    world3dRenderState.colorAttachments[0].blendingEnabled = true;
    world3dRenderState.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    world3dRenderState.depthWriteEnabled = true;
    world3dRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    world3dRenderState.winding = GfxWinding_CCW;
    world3dRenderState.cullMode = GfxCullMode_None;

    principledBrdfPSO = GfxGraphicsPSO::create("principledBrdf", &vertexPosTexCoordNormal, &principledBrdfShaderDesc, &world3dRenderState);
    //
    GfxShaderDesc gridShaderDesc = {};
    gridShaderDesc.shaderSrc = "grid.hlsl";
    gridShaderDesc.vsEntrypoint = "vsGrid";
    gridShaderDesc.fsEntrypoint = "fsGrid";
    
    GfxRenderStateDesc gridRenderState = {};
    gridRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
    gridRenderState.colorAttachments[0].blendingEnabled = true;
    gridRenderState.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    gridRenderState.depthWriteEnabled = true;
    gridRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    gridRenderState.winding = GfxWinding_CCW;
    gridRenderState.cullMode = GfxCullMode_None;
    
    gridPSO = GfxGraphicsPSO::create("gridPSO", nullptr, &gridShaderDesc, &gridRenderState);
    //
    GfxVertexInputDesc vertexPos = {};
    vertexPos.elementCount = 1;
    vertexPos.elements[0].semanticName = "POSITION";
    vertexPos.elements[0].semanticIndex = 0;
    vertexPos.elements[0].offset = 0;
    vertexPos.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexPos.elements[0].bufferIndex = 0;
    vertexPos.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc skyboxShaderDesc = {};
    skyboxShaderDesc.shaderSrc = "skybox.hlsl";
    skyboxShaderDesc.vsEntrypoint = "vsSkybox";
    skyboxShaderDesc.fsEntrypoint = "fsSkybox";
    skyboxShaderDesc.defines = "LEFT";
    
    GfxRenderStateDesc skyboxRenderState = {};
    skyboxRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_R16G16B16A16_SFLOAT;
    skyboxRenderState.colorAttachments[0].blendingEnabled = true;
    skyboxRenderState.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    skyboxRenderState.depthWriteEnabled = true;
    skyboxRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    skyboxRenderState.winding = GfxWinding_CCW;
    skyboxRenderState.cullMode = GfxCullMode_None;

    skyboxPSO = GfxGraphicsPSO::create("skybox", &vertexPos, &skyboxShaderDesc, &skyboxRenderState);
    
    //
    GfxShaderDesc tonemapShaderDesc = {};
    tonemapShaderDesc.shaderSrc = "tonemap.hlsl";
    tonemapShaderDesc.csEntrypoint = "csGenerateHistogram";
    tonemapGenerateHistogramPSO = GfxComputePSO::create("tonemapGenerateHistogram", &tonemapShaderDesc);
    
    tonemapShaderDesc.csEntrypoint = "csClearOutputLuminanceHistogram";
    tonemapClearOutputLuminanceHistogramPSO = GfxComputePSO::create("tonemapClearOutputLuminanceHistogram", &tonemapShaderDesc);
    
    tonemapShaderDesc.csEntrypoint = "csComputeAvgLuminance";
    tonemapComputeAvgLuminancePSO = GfxComputePSO::create("tonemapComputeAvgLuminance", &tonemapShaderDesc);
    
    tonemapShaderDesc.csEntrypoint = "csReinhard";
    tonemapReinhardPSO = GfxComputePSO::create("tonemapReinhard", &tonemapShaderDesc);
    //
    GfxShaderDesc compositeShaderDesc = {};
    compositeShaderDesc.shaderSrc = "composite.hlsl";
    compositeShaderDesc.csEntrypoint = "csComposite";
    compositePSO = GfxComputePSO::create("composite", &compositeShaderDesc);
    
    //
    g_Viewport = rgNew(Viewport);

    // Initialize tonemapper params
    g_GameState->tonemapperMinLogLuminance = -2.0f;
    g_GameState->tonemapperMaxLogLuminance = 10.0f;
    g_GameState->tonemapperAdaptationRate = 1.1f;
    g_GameState->tonemapperExposureKey = 0.15f;
    
    // Initialize show/hide vars
    g_GameState->debugShowGrid = false;
    
    g_PhysicSystem = rgNew(PhysicSystem);
    
    g_GameState->baseColor2DRT = GfxTexture::create("baseColor2DRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_R8G8B8A8_UNORM, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
    
    g_GameState->baseColorRT = GfxTexture::create("baseColorRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_R16G16B16A16_SFLOAT, GfxTextureMipFlag_1Mip, GfxTextureUsage_RenderTarget, nullptr);
    g_GameState->depthStencilRT = GfxTexture::create("depthStencilRT", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D32_SFLOAT, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);
    
    ImageRef sanGiuseppeBridgeCube = loadImage("small_empty_room_1_alb.dds"); // je_gray_02.dds
    sangiuseppeBridgeCubeTex = GfxTexture::create("sangiuseppeBridgeCube", GfxTextureDim_Cube, sanGiuseppeBridgeCube->width, sanGiuseppeBridgeCube->height, sanGiuseppeBridgeCube->format, GfxTextureMipFlag_1Mip, GfxTextureUsage_ShaderRead, sanGiuseppeBridgeCube->slices);
    
    ImageRef sanGiuseppeBridgeCubeIrradiance = loadImage("small_empty_room_1_irr.dds"); // je_gray_02_irradiance.dds
    sangiuseppeBridgeCubeIrradianceTex = GfxTexture::create("sangiuseppeBridgeCubeIrradiance", GfxTextureDim_Cube, sanGiuseppeBridgeCubeIrradiance->width, sanGiuseppeBridgeCubeIrradiance->height, sanGiuseppeBridgeCubeIrradiance->format, GfxTextureMipFlag_1Mip, GfxTextureUsage_ShaderRead, sanGiuseppeBridgeCubeIrradiance->slices);
    
    
    ///
    ImageRef japaneseStoneWall1k = loadImage("japanese_stone_wall_1k/japanese_stone_wall_diff_1k.png");
    japaneseStoneWallDiff1kTex = GfxTexture::create("japanese_stone_wall_diff_1k", GfxTextureDim_2D, japaneseStoneWall1k->width, japaneseStoneWall1k->height, japaneseStoneWall1k->format, GfxTextureMipFlag_GenMips, GfxTextureUsage_ShaderRead, japaneseStoneWall1k->slices);
    ///

    skyboxVertexBuffer = GfxBuffer::create("skyboxVertexBuffer", GfxMemoryType_Default, g_SkyboxVertices, sizeof(g_SkyboxVertices), GfxBufferUsage_VertexBuffer);
    outputLuminanceHistogramBuffer = GfxBuffer::create("luminanceHistogramAndAvg", GfxMemoryType_Default, nullptr, (sizeof(uint32_t) * LUMINANCE_HISTOGRAM_BINS_COUNT) + (sizeof(float) * (1 + 1)), GfxBufferUsage_ShaderRW);

#if 0
    b2Vec2 gravity(0.0f, -9.8f);
    g_GameData->phyWorld = rgNew(b2World)(gravity);
    b2World& world = *g_GameData->phyWorld;

    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0.0f, -10.0f);
    b2Body* groundBody = world.CreateBody(&groundBodyDef);

    b2PolygonShape groundBox;
    groundBox.SetAsBox(50.0f, 10.0f);
    groundBody->CreateFixture(&groundBox, 0.0f);

    b2BodyDef boxBodyDef;
    boxBodyDef.type = b2_dynamicBody;
    boxBodyDef.position.Set(0.0f, 4.0f);
    b2Body* boxBody = world.CreateBody(&boxBodyDef);

    b2PolygonShape boxShape;
    boxShape.SetAsBox(1.0f, 1.0f);
    b2FixtureDef boxFixtureDef;
    boxFixtureDef.shape = &boxShape;
    boxFixtureDef.density = 1.0f;
    boxFixtureDef.friction = 0.3f;
    boxBody->CreateFixture(&boxFixtureDef);
#endif

    inconFont = loadFont("fonts/inconsolata_26.fnt");
    Glyph aGlyph = inconFont->glyphs['A'];
    return 0;
}

static bool showPostFXEditor = true;
static bool showImGuiDemo = false;
static void showDebugInterface(bool* open)
{
    static int location = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if(location >= 0)
    {
        const float padding = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 workSize = viewport->WorkSize;
        ImVec2 windowPos, windowPosPivot;
        windowPos.x = (location & 1) ? (workPos.x + workSize.x - padding) : (workPos.x + padding);
        windowPos.y = (location & 2) ? (workPos.y + workSize.y - padding) : (workPos.y + padding);
        windowPosPivot.x = (location & 1) ? 1.0f : 0.0f;
        windowPosPivot.y = (location & 2) ? 1.0f : 0.0f;
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPosPivot);
        windowFlags |= ImGuiWindowFlags_NoMove;
    }
    ImGui::SetNextWindowBgAlpha(0.2f); // Transparent background
    if(ImGui::Begin("RG DebugInterface", open, windowFlags))
    {
        //static f64 lastFewDeltaTs[60] = { 0 };
        //static i32 deltaTBufferIndex = 0;
        //lastFewDeltaTs[deltaTBufferIndex] = g_DeltaTime;
        //deltaTBufferIndex = (deltaTBufferIndex + 1) % rgARRAY_COUNT(lastFewDeltaTs);
        //f64 lastFewDeltaTSum = 0;
        //for(i32 i = 0; i < rgARRAY_COUNT(lastFewDeltaTs); ++i)
        //{
        //    lastFewDeltaTSum += lastFewDeltaTs[i];
        //}
        //ImGui::Text("%0.1f FPS (Avg)", 1.0 / (lastFewDeltaTSum / rgARRAY_COUNT(lastFewDeltaTs)));
        
        ImGui::Text("%0.1f FPS (Avg)", io.Framerate);
        ImGui::Separator();

        ImGui::Text("GameLib");
        
        /*if(ImGui::IsMousePosValid())
        {
            ImGui::Text("Mouse Position: (%.1f,%.1f)", io.MousePos.x, io.MousePos.y);
        }
        else
        {
            ImGui::Text("Mouse Position: <invalid>");
        }*/

        if(ImGui::BeginPopupContextWindow())
        {
            if(ImGui::MenuItem("debugShowGrid", NULL, g_GameState->debugShowGrid))
            {
                g_GameState->debugShowGrid = !g_GameState->debugShowGrid;
            }
            
            if(ImGui::MenuItem("PostFX Editor", NULL, showPostFXEditor))
            {
                showPostFXEditor = !showPostFXEditor;
            }

            if(ImGui::MenuItem("ImGui Demo", NULL, showImGuiDemo))
            {
                showImGuiDemo = !showImGuiDemo;
            }

            if(open && ImGui::MenuItem("Close")) *open = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

i32 updateAndDraw(f64 dt)
{
    //rgLog("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);
    if(showImGuiDemo) { ImGui::ShowDemoWindow(&showImGuiDemo); }
    showDebugInterface(NULL);
    
    g_Viewport->tick();

    // PREPARE COMMON RESOURCES & DATA
    struct CommonParams
    {
        f32 cameraBasisMatrix[9];
        f32 _padding1[3];
        f32 cameraViewMatrix[16];
        f32 cameraProjMatrix[16];
        f32 cameraViewProjMatrix[16];
        f32 cameraInvViewMatrix[16];
        f32 cameraInvProjMatrix[16];
        f32 cameraViewRotOnlyMatrix[16];
        f32 cameraNear;
        f32 cameraFar;
        f32 timeDelta;
        f32 timeGame;
    } commonParams;
    
    copyMatrix3ToFloatArray(commonParams.cameraBasisMatrix, g_Viewport->cameraBasis);
    copyMatrix4ToFloatArray(commonParams.cameraViewMatrix, g_Viewport->cameraView);
    copyMatrix4ToFloatArray(commonParams.cameraProjMatrix, g_Viewport->cameraProjection);
    copyMatrix4ToFloatArray(commonParams.cameraViewProjMatrix, g_Viewport->cameraViewProjection);
    copyMatrix4ToFloatArray(commonParams.cameraInvViewMatrix, g_Viewport->cameraInvView);
    copyMatrix4ToFloatArray(commonParams.cameraInvProjMatrix, g_Viewport->cameraInvProjection);
    copyMatrix4ToFloatArray(commonParams.cameraViewRotOnlyMatrix, g_Viewport->cameraViewRotOnly);
    commonParams.cameraNear = g_Viewport->cameraNear;
    commonParams.cameraFar  = g_Viewport->cameraFar;
    commonParams.timeDelta = (f32)theAppInput->deltaTime;
    commonParams.timeGame  = (f32)theAppInput->time;

    GfxFrameResource commonParamsBuffer = gfxGetFrameAllocator()->newBuffer("commonParams", sizeof(commonParams), &commonParams);
    
    // RENDER SIMPLE 2D STUFF
    {
        g_GameState->characterPortraits.erase(g_GameState->characterPortraits.begin(), g_GameState->characterPortraits.end());
        for(i32 i = 0; i < 4; ++i)
        {
            for(i32 j = 0; j < 4; ++j)
            {
                f32 px = (f32)(j * (100) + 10 * (j + 1) + sin(theAppInput->time) * 30);
                f32 py = (f32)(i * (100) + 10 * (i + 1) + cos(theAppInput->time) * 30);
                
                pushTexturedQuad(&g_GameState->characterPortraits, SpriteLayer_0, defaultQuadUV, {px, py, 100.0f, 100.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, debugTextureHandles[j + i * 4]);
            }
        }
        pushTexturedQuad(&g_GameState->characterPortraits, SpriteLayer_0, defaultQuadUV, {200.0f, 300.0f, 447.0f, 400.0f}, 0xFFFFFFFF, {0, 0, 0, 0}, g_GameState->flowerTexture);
        
        pushText(&g_GameState->characterPortraits, 600, 500, inconFont, 1.0f, "Hello from rg_gamelib");
        
        GfxRenderPass simple2dRenderPass = {};
        simple2dRenderPass.colorAttachments[0].texture = g_GameState->baseColor2DRT;
        simple2dRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dRenderPass.colorAttachments[0].clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
        simple2dRenderPass.depthStencilAttachmentTexture = g_GameState->depthStencilRT;
        simple2dRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dRenderPass.clearDepth = 1.0f;
        
        GfxRenderCmdEncoder* simple2dRenderEncoder = gfxSetRenderPass("Simple2D Pass", &simple2dRenderPass);
        simple2dRenderEncoder->setGraphicsPSO(simple2DPSO);
        simple2dRenderEncoder->drawTexturedQuads(&g_GameState->characterPortraits, nullptr, nullptr);
        simple2dRenderEncoder->end();
    }
     
    // 1. demo scene render - draw ground plane and shaderball instances
    {
        GfxRenderPass sceneForwardPass = {};
        sceneForwardPass.colorAttachments[0].texture = g_GameState->baseColorRT;
        sceneForwardPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        sceneForwardPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        sceneForwardPass.colorAttachments[0].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        sceneForwardPass.depthStencilAttachmentTexture = g_GameState->depthStencilRT;
        sceneForwardPass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        sceneForwardPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* sceneFowardRenderEncoder = gfxSetRenderPass("Scene Forward", &sceneForwardPass);
        sceneFowardRenderEncoder->setGraphicsPSO(principledBrdfPSO);
        
        // instance
        struct
        {
            f32 worldXform[256][16];
            f32 invTposeWorldXform[256][16];
        } instanceParams;
        
        Matrix4 xform = Matrix4::identity();
        copyMatrix4ToFloatArray(&instanceParams.worldXform[0][0], xform);
        copyMatrix4ToFloatArray(&instanceParams.invTposeWorldXform[0][0], transpose(inverse(xform)));
        
        
        GfxFrameResource demoSceneMeshInstanceParams = gfxGetFrameAllocator()->newBuffer("demoSceneMeshInstanceParams", sizeof(instanceParams), &instanceParams);
        
        sceneFowardRenderEncoder->bindBuffer("commonParams", &commonParamsBuffer);
        sceneFowardRenderEncoder->bindBuffer("instanceParams", &demoSceneMeshInstanceParams);
        sceneFowardRenderEncoder->bindTexture("diffuseTexMap", japaneseStoneWallDiff1kTex);
        sceneFowardRenderEncoder->bindTexture("irradianceMap", sangiuseppeBridgeCubeIrradianceTex);
        sceneFowardRenderEncoder->bindSamplerState("irradianceSampler", GfxState::samplerBilinearClampEdge);
        
        for(i32 i = 0; i < g_GameState->shaderballModel->meshes.size(); ++i)
        {
            Mesh* m = &g_GameState->shaderballModel->meshes[i];
            
            sceneFowardRenderEncoder->setVertexBuffer(g_GameState->shaderballModel->vertexIndexBuffer, g_GameState->shaderballModel->vertexBufferOffset +  m->vertexDataOffset, 0);
            if(m->properties & MeshProperties_Has32BitIndices)
            {
                sceneFowardRenderEncoder->drawIndexedTriangles(m->indexCount, true, g_GameState->shaderballModel->vertexIndexBuffer, g_GameState->shaderballModel->index32BufferOffset + m->indexDataOffset, 1);
            }
            else
            {
                sceneFowardRenderEncoder->drawIndexedTriangles(m->indexCount, false, g_GameState->shaderballModel->vertexIndexBuffer, g_GameState->shaderballModel->index32BufferOffset + m->indexDataOffset, 1);
            }
        }

        sceneFowardRenderEncoder->end();
    }
    
    // RENDER GRID AND EDITOR STUFF
    {
        GfxRenderPass skyboxRenderPass = {};
        skyboxRenderPass.colorAttachments[0].texture = g_GameState->baseColorRT;
        skyboxRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Load;
        skyboxRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        skyboxRenderPass.colorAttachments[0].clearColor = { 1.0f, 1.0f, 0.0f, 1.0f };
        skyboxRenderPass.depthStencilAttachmentTexture = g_GameState->depthStencilRT;
        skyboxRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        skyboxRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* skyboxRenderEncoder = gfxSetRenderPass("Skybox Pass", &skyboxRenderPass);
        skyboxRenderEncoder->setGraphicsPSO(skyboxPSO);
        skyboxRenderEncoder->bindBuffer("commonParams", &commonParamsBuffer);
        skyboxRenderEncoder->bindTexture("diffuseCubeMap", sangiuseppeBridgeCubeTex);
        skyboxRenderEncoder->bindSamplerState("skyboxSampler", GfxState::samplerBilinearClampEdge);
        skyboxRenderEncoder->setVertexBuffer(skyboxVertexBuffer, 0, 0);
        skyboxRenderEncoder->drawTriangles(0, 36, 1);
        skyboxRenderEncoder->end();
        
        if(g_GameState->debugShowGrid)
        {
            GfxRenderPass gridRenderPass = {};
            gridRenderPass.colorAttachments[0].texture = g_GameState->baseColorRT;
            gridRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Load;
            gridRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
            gridRenderPass.depthStencilAttachmentTexture = g_GameState->depthStencilRT;
            gridRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
            gridRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
            
            GfxRenderCmdEncoder* gridRenderEncoder = gfxSetRenderPass("DemoScene Pass", &gridRenderPass);
            gridRenderEncoder->setGraphicsPSO(gridPSO);
            gridRenderEncoder->bindBuffer("commonParams", &commonParamsBuffer);
            gridRenderEncoder->drawTriangles(0, 6, 1);
            gridRenderEncoder->end();
        }
        
        {
            if(showPostFXEditor)
            {
                ImGui::SetNextWindowBgAlpha(0.0f); // Transparent background
                if(ImGui::Begin("PostFX Editor", &showPostFXEditor))
                {
                    ImGui::DragFloatRange2("LogLuminance", &g_GameState->tonemapperMinLogLuminance, &g_GameState->tonemapperMaxLogLuminance, 0.1f, -20.0f, 20.0f, "Min: %.1f", "Max: %.1f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::InputFloat("ExposureKey", &g_GameState->tonemapperExposureKey, 0.01f);
                    ImGui::InputFloat("AdaptationRate", &g_GameState->tonemapperAdaptationRate, 0.1f);
                    ImGui::SeparatorText("Histogram");
#ifndef RG_D3D12_RNDR
                    u8* luminanceOutputBuffer = (u8*)outputLuminanceHistogramBuffer->map(0, 0);
                    
                    u32* histogramData = (u32*)(luminanceOutputBuffer + LUMINANCE_BUFFER_OFFSET_HISTOGRAM);
                    float histogram[LUMINANCE_HISTOGRAM_BINS_COUNT];
                    for(int i = 0; i < rgArrayCount(histogram); ++i)
                    {
                        histogram[i] = histogramData[i];
                    }
                    
                    ImGui::PlotHistogram("##luminanceHistogram", histogram, rgArrayCount(histogram), 0, "luminanceHistogram", FLT_MAX, FLT_MAX, ImVec2(LUMINANCE_HISTOGRAM_BINS_COUNT * (LUMINANCE_BLOCK_SIZE < 32 ? 2 : 1), 100));
                    f32 adaptedLuminance = *(f32*)(luminanceOutputBuffer + LUMINANCE_BUFFER_OFFSET_LUMINANCE);
                    f32 exposure = *(f32*)(luminanceOutputBuffer + LUMINANCE_BUFFER_OFFSET_EXPOSURE);
                    ImGui::Text("Adapted Luminance is %0.2f, and exposure is %0.2f", adaptedLuminance, exposure);
                    
                    outputLuminanceHistogramBuffer->unmap();
#endif // !RG_D3D12_RNDR
                }
                ImGui::End();
            }
     
            struct TonemapParams
            {
                u32   inputImageWidth;
                u32   inputImageHeight;
                u32   inputImagePixelCount;
                float   minLogLuminance;
                float   logLuminanceRange;
                float   oneOverLogLuminanceRange;
                float   luminanceAdaptationKey;
                float   tau;
            } tonemapParams;
            
            tonemapParams.inputImageWidth = g_WindowInfo.width;
            tonemapParams.inputImageHeight = g_WindowInfo.height;
            tonemapParams.inputImagePixelCount = g_WindowInfo.width * g_WindowInfo.height;
            tonemapParams.minLogLuminance = g_GameState->tonemapperMinLogLuminance;
            tonemapParams.logLuminanceRange = g_GameState->tonemapperMaxLogLuminance - g_GameState->tonemapperMinLogLuminance;
            tonemapParams.oneOverLogLuminanceRange = 1.0f / (g_GameState->tonemapperMaxLogLuminance - g_GameState->tonemapperMinLogLuminance);
            tonemapParams.luminanceAdaptationKey = g_GameState->tonemapperExposureKey;
            tonemapParams.tau = g_GameState->tonemapperAdaptationRate;
            
            GfxComputeCmdEncoder* postfxCmdEncoder = gfxSetComputePass("PostFx Pass");
                        
            postfxCmdEncoder->setComputePSO(tonemapClearOutputLuminanceHistogramPSO);
            postfxCmdEncoder->bindBuffer("outputBuffer", outputLuminanceHistogramBuffer, 0);
            postfxCmdEncoder->dispatch(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1);
            
            postfxCmdEncoder->setComputePSO(tonemapGenerateHistogramPSO);
            postfxCmdEncoder->bindTexture("inputImage", g_GameState->baseColorRT);
            //postfxCmdEncoder->bindTexture("outputImage", gfx::getCurrentRenderTargetColorBuffer());
            postfxCmdEncoder->bindBufferFromData("TonemapParams", sizeof(tonemapParams), &tonemapParams);
            postfxCmdEncoder->bindBuffer("outputBuffer", outputLuminanceHistogramBuffer, 0);
            postfxCmdEncoder->dispatch(g_WindowInfo.width, g_WindowInfo.height, 1);

            postfxCmdEncoder->setComputePSO(tonemapComputeAvgLuminancePSO);
            postfxCmdEncoder->bindBuffer("commonParams", &commonParamsBuffer);
            postfxCmdEncoder->bindBufferFromData("TonemapParams", sizeof(tonemapParams), &tonemapParams);
            postfxCmdEncoder->bindBuffer("outputBuffer", outputLuminanceHistogramBuffer, 0);
            postfxCmdEncoder->dispatch(LUMINANCE_BLOCK_SIZE, LUMINANCE_BLOCK_SIZE, 1);
            
            postfxCmdEncoder->setComputePSO(tonemapReinhardPSO);
            postfxCmdEncoder->bindTexture("inputImage", g_GameState->baseColorRT);
            postfxCmdEncoder->bindTexture("outputImage", gfxGetBackbufferTexture());
            //postfxCmdEncoder->bindBufferFromData("TonemapParams", sizeof(tonemapParams), &tonemapParams);
            postfxCmdEncoder->bindBuffer("outputBuffer", outputLuminanceHistogramBuffer, 0);
            postfxCmdEncoder->dispatch(g_WindowInfo.width, g_WindowInfo.height, 1);
            
            //.....
            struct
            {
                uint32_t inputImageDim[2];
            } compositeParams;
            
            compositeParams.inputImageDim[0] = g_WindowInfo.width;
            compositeParams.inputImageDim[1] = g_WindowInfo.height;
            postfxCmdEncoder->setComputePSO(compositePSO);
            postfxCmdEncoder->bindTexture("inputImage", g_GameState->baseColor2DRT);
            postfxCmdEncoder->bindTexture("outputImage", gfxGetBackbufferTexture());
            postfxCmdEncoder->bindBufferFromData("CompositeParams", sizeof(compositeParams), &compositeParams);
            postfxCmdEncoder->dispatch(g_WindowInfo.width, g_WindowInfo.height, 1);
            //
            
            //postfxCmdEncoder->updateFence();
            postfxCmdEncoder->end();
        }
    }
    
    rgHash a = rgCRC32("hello world");

#if 0
    f32 timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 6;
    int32 positionIterations = 2;

    g_GameData->phyWorld->Step(timeStep, velocityIterations, positionIterations);
    g_GameData->phyWorld->Dump();
#endif

    return 0;
}

class Demo3DApp : public TheApp
{
    void setup() override
    {
        ::setup();
    }
    
    void updateAndDraw() override
    {
        ::updateAndDraw(theAppInput->deltaTime);
    }
};

THE_APP_MAIN(Demo3DApp)
