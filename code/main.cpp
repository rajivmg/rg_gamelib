#define RG_H_IMPLEMENTATION
#include "rg.h"

#include "rg_gfx.h"
#include "rg_physic.h"

#include "box2d/box2d.h"

#include <EASTL/vector.h>
#include "game/game.h"

#include "shaders/metal/simple2d_shader.inl"

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

    GfxBuffer* b = gfx::findOrCreateBuffer("dummyBuffer", nullptr, 100, GfxBufferUsage_ConstantBuffer, true);

    GfxTexture2D* t2dptr = gfx::createTexture2D("tiny.tga", loadTexture("tiny.tga"), true, GfxTextureUsage_ShaderRead);
    
    for(rgInt i = 1; i <= 16; ++i)
    {
        char path[256];
        snprintf(path, 256, "debugTextures/textureSlice%d.png", i);
        GfxTexture2D* t2d = gfx::createTexture2D(path, loadTexture(path), true, GfxTextureUsage_ShaderRead);
        gfx::debugTextureHandles.push_back(t2d);
    }
    
    g_GameData->oceanTileTexture = gfx::createTexture2D("ocean_tile", loadTexture("ocean_tile.png"), true, GfxTextureUsage_ShaderRead);
    
    g_GameData->flowerTexture = gfx::createTexture2D("flower", loadTexture("flower.png"), true, GfxTextureUsage_ShaderRead);
    
    //gfxDestroyBuffer("ocean_tile");

    /*
        gfx::setRenderPass();
        gfx::genMips(GfxTexture* obj);
        gfx::texturedQuad();

        ///GfxRenderCmdList* cmdlist = gfx::getRenderCmdList("");
        GfxRenderCmdList* cmdlist = gfx::setRenderPass();
        cmdlist->setPipeline();
        cmdList->setViewport();
        cmdList->drawTexturedQuads();
        cmdlist->endList();
    */

    GfxVertexInputDesc vertexDesc = {};
    vertexDesc.elementsCount = 2;
    vertexDesc.elements[0].semanticName = "POSITION";
    vertexDesc.elements[0].semanticIndex = 0;
    vertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexDesc.elements[0].slot = 0;
    vertexDesc.elements[1].semanticName = "COLOR";
    vertexDesc.elements[1].semanticIndex = 0;
    vertexDesc.elements[1].format = TinyImageFormat_R32G32B32A32_SFLOAT;
    vertexDesc.elements[1].slot = 0;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrcCode = g_Simple2DShaderSrcCode;
    simple2dShaderDesc.vsEntryPoint = "simple2d_VS";
    simple2dShaderDesc.fsEntryPoint = "simple2d_FS";
    simple2dShaderDesc.macros = "RIGHT";
    
    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    
    gfx::createGraphicsPSO("simple2d", &vertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);

    gfx::createSamplerState("nearestRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, true);

    //
    
    {
        for(rgInt y = -50; y < 50; ++y)
        {
            for(rgInt x = -50; x < 50; ++x)
            {
//                pushTexturedQuad(&g_GameData->terrainAndOcean, defaultQuadUV, {(rgFloat)x, (rgFloat)y, 64.0f, 64.0f}, {0, 0, 0, 0}, g_GameData->oceanTileTexture);
            }
        }
         //g_GameData->terrainAndOcean
    }
    
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
    rgLog("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);

    QuadUV fullQuadUV = createQuadUV(0, 0, 512, 512, 512, 512);

    eastl::vector<QuadUV> quadUVs;
    quadUVs.push_back(fullQuadUV);

    {        
        GfxRenderPass simple2dPass = {};
        simple2dPass.colorAttachments[0].texture = gfx::renderTarget[g_FrameIndex];
        simple2dPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dPass.colorAttachments[0].clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };
        simple2dPass.depthStencilAttachmentTexture = gfx::depthStencilBuffer;
        simple2dPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dPass.clearDepth = 0.0f;
        
        GfxRenderCmdEncoder* renderCmdEncoder = gfx::setRenderPass(&simple2dPass, "Simple2D Pass");

        renderCmdEncoder->setGraphicsPSO(gfx::findGraphicsPSO("simple2d"));
        renderCmdEncoder->drawTexturedQuads(&g_GameData->terrainAndOcean);

        
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
        
        renderCmdEncoder->drawTexturedQuads(&g_GameData->characterPortraits);
        
        renderCmdEncoder->end();
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
    g_WindowInfo.width = 720;
    g_WindowInfo.height = 720;

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
        rg::updateAndDraw(g_DeltaTime);
        gfx::endFrame();
    }

    gfx::destroy();
    SDL_DestroyWindow(gfx::mainWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}
