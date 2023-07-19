#define RG_H_IMPLEMENTATION
#include "rg.h"

#include "rg_gfx.h"
#include "rg_physic.h"

#include "backends/imgui_impl_sdl2.h"

#include "box2d/box2d.h"

#include <EASTL/vector.h>
#include "game/game.h"

#include "shaders/metal/simple2d_shader.inl"
#include "shaders/metal/principledbrdf_shader.inl"

// TODO:
// 2. Add rgLogDebug() rgLogError()
// 3. Then use the suitable rgLogXXX() version in VKDbgReportCallback Function based on msgType 
// 4. Integrate EASTL
//

using namespace rg;

// Important: make this a pointer, otherwise if a type with constructor is added to struct, the compiler will complain because it will try to call the constructor of anonymous structs
//rg::GfxCtx* rg::g_GfxCtx;

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    //rgLog("new[] size=%ld alignment=%ld alignmentOffset=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
    //    size, alignment, alignmentOffset, pName, flags, debugFlags, file, line);
    return new uint8_t[size];
}

void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
    //rgLog("new[] size=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
    //    size, name, flags, debugFlags, file, line);
    return new uint8_t[size];
}

GameData* g_GameData;
rg::PhysicSystem* g_PhysicSystem;
rg::WindowInfo g_WindowInfo;

rgDouble g_DeltaTime;
rgDouble g_Time;

rgInt g_FrameIndex;

rgBool g_ShouldQuit;

rgU32 rgRenderKey(rgBool top)
{
    return top ? 1 : 0;
}

rgInt rg::setup()
{
    g_GameData = rgNew(GameData);

    TextureRef tinyTexture = loadTexture("tiny.tga");
    gfx::texture2D->create("tiny", tinyTexture->buf, tinyTexture->width, tinyTexture->height, tinyTexture->format, false, GfxTextureUsage_ShaderRead);
    gfx::texture2D->destroy(rgCRC32("tiny"));
    
    for(rgInt i = 1; i <= 16; ++i)
    {
        char path[256];
        snprintf(path, 256, "debugTextures/textureSlice%d.png", i);
        TextureRef t = loadTexture(path);
        GfxTexture2D* t2d = gfx::texture2D->create(path, t->buf, t->width, t->height, t->format, false, GfxTextureUsage_ShaderRead);
        gfx::debugTextureHandles.push_back(t2d);
    }
    
    TextureRef flowerTex = rg::loadTexture("flower.png");
    g_GameData->flowerTexture = gfx::texture2D->create("flower", flowerTex->buf, flowerTex->width, flowerTex->height, flowerTex->format, false, GfxTextureUsage_ShaderRead);
    
    //gfxDestroyBuffer("ocean_tile");
    g_GameData->shaderballModel = rg::loadModel("shaderball.xml");
    
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
    vertexDesc.elements[2].format = TinyImageFormat_R32G32B32A32_SFLOAT;
    vertexDesc.elements[2].bufferIndex = 0;
    vertexDesc.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrc = "simple2d.hlsl";
    simple2dShaderDesc.vsEntrypoint = "vsSimple2d";
    simple2dShaderDesc.fsEntrypoint = "fsSimple2d";
    simple2dShaderDesc.defines = "RIGHT";
    
    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    //simple2dRenderStateDesc.triangleFillMode = GfxTriangleFillMode_Lines;
    
    gfx::graphicsPSO->create("simple2d", &vertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    
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
    principledBrdfShaderDesc.defines = "LEFT";
    
    GfxRenderStateDesc world3dRenderState = {};
    world3dRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    world3dRenderState.colorAttachments[0].blendingEnabled = true;
    world3dRenderState.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    world3dRenderState.depthWriteEnabled = true;
    world3dRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    world3dRenderState.winding = GfxWinding_CCW;
    world3dRenderState.cullMode = GfxCullMode_None;

    gfx::graphicsPSO->create("principledBrdf", &vertexPosTexCoordNormal, &principledBrdfShaderDesc, &world3dRenderState);
    //
    GfxShaderDesc gridShaderDesc = {};
    gridShaderDesc.shaderSrc = "grid.hlsl";
    gridShaderDesc.vsEntrypoint = "vsGrid";
    gridShaderDesc.fsEntrypoint = "fsGrid";
    
    GfxRenderStateDesc gridRenderState = {};
    gridRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    gridRenderState.colorAttachments[0].blendingEnabled = true;
    gridRenderState.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    gridRenderState.depthWriteEnabled = true;
    gridRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    gridRenderState.winding = GfxWinding_CCW;
    gridRenderState.cullMode = GfxCullMode_None;
    
    gfx::graphicsPSO->create("gridPSO", nullptr, &gridShaderDesc, &gridRenderState);
    //
    

    gfx::samplerState->create("nearestRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, true);
    
    g_PhysicSystem = rgNew(PhysicSystem);

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

    return 0;
}

rgInt rg::updateAndDraw(rgDouble dt)
{
    //rgLog("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);
    ImGui::ShowDemoWindow();
    
    {        
        GfxRenderPass simple2dPass = {};
        simple2dPass.colorAttachments[0].texture = gfx::renderTarget[g_FrameIndex];
        simple2dPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dPass.colorAttachments[0].clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };
        simple2dPass.depthStencilAttachmentTexture = gfx::depthStencilBuffer;
        simple2dPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dPass.clearDepth = 1.0f;
        
        GfxRenderCmdEncoder* textured2dRenderEncoder = gfx::setRenderPass(&simple2dPass, "Simple2D Pass");

        textured2dRenderEncoder->setGraphicsPSO(gfx::graphicsPSO->find("simple2d"_rh));
        textured2dRenderEncoder->drawTexturedQuads(&g_GameData->terrainAndOcean);

        g_GameData->characterPortraits.resize(0);
        for(rgInt i = 0; i < 4; ++i)
        {
            for(rgInt j = 0; j < 4; ++j)
            {
                rgFloat px = j * (100) + 10 * (j + 1) + sinf((rgFloat)g_Time) * 30;
                rgFloat py = i * (100) + 10 * (i + 1) + cosf((rgFloat)g_Time) * 30;
                
                pushTexturedQuad(&g_GameData->characterPortraits, defaultQuadUV, {px, py, 100.0f, 100.0f}, {0, 0, 0, 0}, gfx::debugTextureHandles[j + i * 4]);
            }
        }
        pushTexturedQuad(&g_GameData->characterPortraits, defaultQuadUV, {200.0f, 300.0f, 447.0f, 400.0f}, {0, 0, 0, 0}, g_GameData->flowerTexture);
        
        textured2dRenderEncoder->drawTexturedQuads(&g_GameData->characterPortraits);
        textured2dRenderEncoder->end();
    }
    
    // camera
    struct
    {
        rgFloat projectionPerspective[16];
        rgFloat viewCamera[16];
        rgFloat invProjectionPerspective[16];
        rgFloat invViewCamera[16];
    } cameraParams;
    
    Matrix4 projection = gfx::makePerspectiveProjectionMatrix(1.0f, g_WindowInfo.width/g_WindowInfo.height, 0.01f, 1000.0f);
    Matrix4 view = Matrix4::lookAt(Point3(0.0f, 1.0f, 3.0f), Point3(-0.2, 0.9f, 0), Vector3(0, 1.0f, 0));
    Matrix4 invProjection = inverse(projection);
    Matrix4 invView = inverse(view);
    
    copyMatrix4ToFloatArray(cameraParams.projectionPerspective, projection);
    copyMatrix4ToFloatArray(cameraParams.viewCamera, view);
    copyMatrix4ToFloatArray(cameraParams.invProjectionPerspective, invProjection);
    copyMatrix4ToFloatArray(cameraParams.invViewCamera, invView);
    
    GfxFrameResource cameraParamsBuffer = gfx::getFrameAllocator()->newBuffer("cameraParamsCBufferBunny", sizeof(cameraParams), &cameraParams);
    
    {
        GfxRenderPass demoScenePass = {};
        demoScenePass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
        demoScenePass.colorAttachments[0].loadAction = GfxLoadAction_Load;
        demoScenePass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        demoScenePass.depthStencilAttachmentTexture = gfx::getRenderTargetDepthBuffer();
        demoScenePass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        demoScenePass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* demoSceneEncoder = gfx::setRenderPass(&demoScenePass, "DemoScene Pass");
        demoSceneEncoder->setGraphicsPSO(gfx::graphicsPSO->find(rgCRC32("principledBrdf")));
        
        // instance
        struct
        {
            rgFloat worldXform[256][16];
            rgFloat invTposeWorldXform[256][16];
        } instanceParams;
        
        Matrix4 xform = Matrix4(Transform3::rotationY(sinf(g_Time * 0.5f)));
        copyMatrix4ToFloatArray(&instanceParams.worldXform[0][0], xform);
        copyMatrix4ToFloatArray(&instanceParams.invTposeWorldXform[0][0], transpose(inverse(xform)));
        
        
        GfxFrameResource instanceParamsBuffer = gfx::getFrameAllocator()->newBuffer("instanceParamsCBufferBunny", sizeof(instanceParams), &instanceParams);
        
        demoSceneEncoder->bindBuffer(&cameraParamsBuffer, "camera");
        demoSceneEncoder->bindBuffer(&instanceParamsBuffer, "instanceParams");
        
        for(rgInt i = 0; i < g_GameData->shaderballModel->meshes.size(); ++i)
        {
            rg::Mesh* m = &g_GameData->shaderballModel->meshes[i];
            
            demoSceneEncoder->setVertexBuffer(g_GameData->shaderballModel->vertexIndexBuffer, g_GameData->shaderballModel->vertexBufferOffset +  m->vertexDataOffset, 0);
            demoSceneEncoder->drawIndexedTriangles(m->indexCount, true, g_GameData->shaderballModel->vertexIndexBuffer, g_GameData->shaderballModel->index32BufferOffset + m->indexDataOffset, 1);
        }

        demoSceneEncoder->end();
    }
    
    {
        GfxRenderPass gridRenderPass = {};
        gridRenderPass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
        gridRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Load;
        gridRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        gridRenderPass.depthStencilAttachmentTexture = gfx::getRenderTargetDepthBuffer();
        gridRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        gridRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* gridRenderEncoder = gfx::setRenderPass(&gridRenderPass, "DemoScene Pass");
        gridRenderEncoder->setGraphicsPSO(gfx::graphicsPSO->find("gridPSO"_rh));
        gridRenderEncoder->bindBuffer(&cameraParamsBuffer, "camera");
        gridRenderEncoder->drawTriangles(0, 6, 1);
        gridRenderEncoder->end();
    }
    
    rgHash a = rgCRC32("hello world");

#if 0
    rgFloat timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 6;
    int32 positionIterations = 2;

    g_GameData->phyWorld->Step(timeStep, velocityIterations, positionIterations);
    g_GameData->phyWorld->Dump();
#endif

    return 0;
}

rgInt createSDLWindow()
{
    g_WindowInfo.width = 1056.0f;
    g_WindowInfo.height = 594.0f;

    Uint32 windowFlags = 0;
    
#if defined(RG_VULKAN_RNDR)
    windowFlags |= SDL_WINDOW_VULKAN;
#elif defined(RG_METAL_RNDR)
    windowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;
    windowFlags |= SDL_WINDOW_METAL;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#elif defined(RG_OPENGL_RNDR)
    windowFlags |= SDL_WINDOW_OPENGL;
#elif defined(RG_D3D12_RNDR)
    windowFlags |= SDL_WINDOW_RESIZABLE;
#endif
    
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
    {
#if defined(RG_OPENGL_RNDR)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 2);
#endif
        gfx::mainWindow = SDL_CreateWindow("gamelib",
                                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           g_WindowInfo.width, g_WindowInfo.height, windowFlags);
        rgAssert(gfx::mainWindow != NULL);
        return 0;
    }
    
    return -1;
}

int main(int argc, char* argv[])
{
    g_FrameIndex = -1;

    if(createSDLWindow() != 0)
    {
        return -1; // error;
    }

    rgInt gfxPreInitResult = gfx::preInit();
    rgInt gfxInitResult = gfx::init();
    rgInt gfxCommonInitResult = gfx::initCommonStuff();

    if(gfxInitResult || gfxCommonInitResult)
    {
        return gfxInitResult | gfxCommonInitResult;
    }

    rg::setup();

    g_ShouldQuit = false;
    SDL_Event event;
    
    Uint64 currentPerfCounter = SDL_GetPerformanceCounter();
    Uint64 previousPerfCounter = currentPerfCounter;
    
    while(!g_ShouldQuit)
    {
        ++gfx::frameNumber;

        Uint64 counterFrequency = SDL_GetPerformanceFrequency();
        previousPerfCounter = currentPerfCounter;
        currentPerfCounter = SDL_GetPerformanceCounter();
        g_DeltaTime = ((currentPerfCounter - previousPerfCounter) / (1.0 * SDL_GetPerformanceFrequency()));
        g_Time = currentPerfCounter / (1.0 * counterFrequency);
        
        while(SDL_PollEvent(&event) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            if(event.type == SDL_QUIT)
            {
                g_ShouldQuit = true;
            }
            else if(event.type == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                g_WindowInfo.width = event.window.data1;
                g_WindowInfo.height = event.window.data2;
                gfx::onSizeChanged();
            }
            else
            {
                // process event
            }
        }
        
        gfx::startNextFrame();
        
        gfx::rendererImGuiNewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        rg::updateAndDraw(g_DeltaTime);
        
        ImGui::Render();
        gfx::rendererImGuiRenderDrawData();
        gfx::endFrame();
        
    }

    gfx::destroy();
    SDL_DestroyWindow(gfx::mainWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}
