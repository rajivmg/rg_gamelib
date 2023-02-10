#define RG_H_IMPLEMENTATION
#include "rg.h"

#include "rg_gfx.h"

// TODO:
// 2. Add rgLogDebug() rgLogError()
// 3. Then use the suitable rgLogXXX() version in VKDbgReportCallback Function based on msgType 
// 4. Integrate EASTL
//

using namespace rg;

// Important: make this a pointer, otherwise if a type with constructor is added to struct, the compiler will complain because it will try to call the constructor of anonymous structs
rg::GfxCtx rg::g_GfxCtx;

rgInt rg::updateAndDraw(rg::GfxCtx* gtxCtx, rgDouble dt)
{
    printf("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);
    
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
    if(createSDLWindow(&g_GfxCtx) != 0)
    {
        return -1; // error;
    }

    rgInt gfxInitResult = gfxInit();
    if(gfxInitResult)
    {
        return gfxInitResult;
    }

    rgBool shouldQuit = false;
    SDL_Event event;
    
    Uint64 currentPerfCounter = SDL_GetPerformanceCounter();
    Uint64 previousPerfCounter = currentPerfCounter;
    rgDouble deltaTime = 0.0;
    
    while(!shouldQuit)
    {
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

        updateAndDraw(&g_GfxCtx, deltaTime);
        
        gfxDraw();
    }

    SDL_DestroyWindow(g_GfxCtx.mainWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}
