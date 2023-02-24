#define RG_H_IMPLEMENTATION
#include "rg.h"

#include "rg_gfx.h"

#include <EASTL/vector.h>

// TODO:
// 2. Add rgLogDebug() rgLogError()
// 3. Then use the suitable rgLogXXX() version in VKDbgReportCallback Function based on msgType 
// 4. Integrate EASTL
//

using namespace rg;

// Important: make this a pointer, otherwise if a type with constructor is added to struct, the compiler will complain because it will try to call the constructor of anonymous structs
rg::GfxCtx* rg::g_GfxCtx;

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


rgU32 rgRenderKey(rgBool top)
{
    return top ? 1 : 0;
}

rgInt rg::updateAndDraw(rgDouble dt)
{
    printf("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);

    QuadUV fullQuadUV = rg::createQuadUV(0, 0, 512, 512, 512, 512);

    eastl::vector<QuadUV> quadUVs;
    quadUVs.push_back(fullQuadUV);

    printf("%f\n", quadUVs.back().uvBottomRight[1]);

    //gfxTexturedQuad();
    RenderCmdList* cmdList = gfxGetRenderCmdList();
    //GfxCmdList* curGfxCmdList = gfxGetCmdList();
    //GfxCmd_TexturedQuad* texQuadCmd = curGfxCmdList->AddCmd();
    //RenderCmd_TexturedQuad *texQuadCmd = RenderCmdList->AddCmd()
    {
        // Can RenderCmdList be implemented using intrusive_list
        RenderCmdTexturedQuad* texQuadCmd = cmdList->addCmd<RenderCmdTexturedQuad>(rgRenderKey(true), 0);
        texQuadCmd->header.type = RenderCmdType_TexturedQuad;
        TexturePtr tTexture = rg::loadTexture("T.tga"); // TODO: create loadTGA()
        texQuadCmd->texture = gfxNewTexture2D(tTexture, GfxResourceUsage_Read);
        //gfxCtx()->sdl.tTex = texQuadCmd->texture;
    }
    //RenderCmdList* cmdList = gfxBeginRenderCmdList("GameRenderCmdList");
    //gfxTexturedQuad(cmdList, birdTexture, defaultQuadUV, Vector2(30, 100), )
    
    return 0;
}

rgInt createSDLWindow(GfxCtx* ctx)
{
    rgUInt windowWidth = 720;
    rgUInt windowHeight = 720;
    Uint32 windowFlags = 0;
    
#ifdef RG_VULKAN_RNDR
    windowFlags |= SDL_WINDOW_VULKAN;
#elif defined(RG_METAL_RNDR)
    windowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;
    windowFlags |= SDL_WINDOW_METAL;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#endif
    
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
    {
        ctx->mainWindow = SDL_CreateWindow("gamelib",
                                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           windowWidth, windowHeight, windowFlags);
        rgAssert(ctx->mainWindow != NULL);
        return 0;
    }
    
    return -1;
}

int main(int argc, char* argv[])
{
    g_GfxCtx = rgNew(GfxCtx);

    if(createSDLWindow(g_GfxCtx) != 0)
    {
        return -1; // error;
    }

    rgInt gfxInitResult = gfxInit();
    rgInt gfxCommonInitResult = gfxCommonInit();

    if(gfxInitResult || gfxCommonInitResult)
    {
        return gfxInitResult | gfxCommonInitResult;
    }

    rgBool shouldQuit = false;
    SDL_Event event;
    
    Uint64 currentPerfCounter = SDL_GetPerformanceCounter();
    Uint64 previousPerfCounter = currentPerfCounter;
    rgDouble deltaTime = 0.0;
    
    while(!shouldQuit)
    {
        ++g_GfxCtx->frameNumber;

        previousPerfCounter = currentPerfCounter;
        currentPerfCounter = SDL_GetPerformanceCounter();
        deltaTime = ((currentPerfCounter - previousPerfCounter) / (1.0 * SDL_GetPerformanceFrequency()));
        
        while(SDL_PollEvent(&event) != 0)
        {
            if(event.type == SDL_QUIT)
            {
                shouldQuit = true;
            }
            else
            {
                // process event
            }
        }

        updateAndDraw(deltaTime);
        
        gfxDraw();
    }

    SDL_DestroyWindow(g_GfxCtx->mainWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}
