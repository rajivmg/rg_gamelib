#include "core.h"
#include "gfx.h"

#include "backends/imgui_impl_sdl2.h"

rgDouble    g_DeltaTime;
rgDouble    g_Time;
SDL_Window* g_AppMainWindow;
rgBool      g_ShouldAppQuit;
rgUInt      g_FrameNumber;

// EASTL MEMORY OVERLOADS
// ----------------------

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

// THE APP
// -------
#if 1
extern GameInput* g_GameInput;
static GameInput* oldGameInput, *newGameInput;

static rgBool processGameButtonState(GameButtonState* newButtonState, rgBool isDown)
{
    if(newButtonState->endedDown != isDown)
    {
        newButtonState->endedDown = isDown;
        ++newButtonState->halfTransitionCount;
        return true;
    }
    return false;
}

static rgBool processGameInputs(SDL_Event* event, GameInput* gameInput)
{
    GameControllerInput* controller1 = &gameInput->controllers[0];
    
    switch(event->type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            if(!event->key.repeat)
            {
                rgBool isDown = (event->key.state == SDL_PRESSED);
                switch(event->key.keysym.sym)
                {
                    case SDLK_w:
                    case SDLK_UP:
                    {
                        processGameButtonState(&controller1->forward, isDown);
                    } break;
                        
                    case SDLK_s:
                    case SDLK_DOWN:
                    {
                        processGameButtonState(&controller1->backward, isDown);
                    } break;
                        
                    case SDLK_a:
                    case SDLK_LEFT:
                    {
                        processGameButtonState(&controller1->left, isDown);
                    } break;
                        
                    case SDLK_d:
                    case SDLK_RIGHT:
                    {
                        processGameButtonState(&controller1->right, isDown);
                    } break;
                    
                    case SDLK_q:
                    case SDLK_c:
                    {
                        processGameButtonState(&controller1->up, isDown);
                    } break;
                        
                    case SDLK_e:
                    case SDLK_f:
                    {
                        processGameButtonState(&controller1->down, isDown);
                    } break;
                    
                    case SDLK_ESCAPE:
                    {
                        SDL_Event quitEvent;
                        quitEvent.type = SDL_QUIT;
                        SDL_PushEvent(&quitEvent);
                    } break;
                }
            }
        } break;
            
        case SDL_MOUSEMOTION:
        {
            gameInput->mouse.x = event->motion.x;
            gameInput->mouse.y = event->motion.y;
            gameInput->mouse.relX = event->motion.xrel;
            gameInput->mouse.relY = event->motion.yrel;
        } break;
            
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
        {
            rgBool isDown = (event->button.state == SDL_PRESSED);
            switch(event->button.button)
            {
                case SDL_BUTTON_LEFT:
                {
                    processGameButtonState(&gameInput->mouse.left, isDown);
                } break;
                    
                case SDL_BUTTON_MIDDLE:
                {
                    processGameButtonState(&gameInput->mouse.middle, isDown);
                } break;
                    
                case SDL_BUTTON_RIGHT:
                {
                    processGameButtonState(&gameInput->mouse.right, isDown);
                } break;
            }
        } break;
    }
    return true;
}

static rgInt createSDLWindow()
{
#if 0
    g_WindowInfo.width = 1056;
    g_WindowInfo.height = 594;
#else
    g_WindowInfo.width = 1280;
    g_WindowInfo.height = 720;
#endif

    Uint32 windowFlags = 0;
    
#if defined(RG_VULKAN_RNDR)
    windowFlags |= SDL_WINDOW_VULKAN;
#elif defined(RG_METAL_RNDR)
    // NOTE: If memory leaks occure when the window is occluded enable this line
    //windowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;
    windowFlags |= SDL_WINDOW_METAL;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#elif defined(RG_D3D12_RNDR)
    windowFlags |= SDL_WINDOW_RESIZABLE;
    //windowFlags |= SDL_WINDOW_FULLSCREEN;
#endif
    
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
    {
        g_AppMainWindow = SDL_CreateWindow("gamelib",
                                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           g_WindowInfo.width, g_WindowInfo.height, windowFlags);
        rgAssert(g_AppMainWindow != NULL);
        return 0;
    }
    
    return -1;
}

int TheApp::beginApp()
{
    g_FrameIndex = -1;

    if(createSDLWindow() != 0)
    {
        return -1; // error;
    }

    rgInt gfxPreInitResult = gfxPreInit();
    rgInt gfxInitResult = gfxInit();
    rgInt gfxCommonInitResult = gfxPostInit();

    if(gfxInitResult || gfxCommonInitResult)
    {
        return gfxInitResult | gfxCommonInitResult;
    }

    //setup();

    g_ShouldAppQuit = false;
    
    currentPerfCounter = SDL_GetPerformanceCounter();
    previousPerfCounter = currentPerfCounter;
    
    oldGameInput = &inputs[0];
    newGameInput = &inputs[1];
}

void TheApp::beforeUpdateAndDraw()
{
    ++g_FrameNumber;

    Uint64 counterFrequency = SDL_GetPerformanceFrequency();
    previousPerfCounter = currentPerfCounter;
    currentPerfCounter = SDL_GetPerformanceCounter();
    g_DeltaTime = ((currentPerfCounter - previousPerfCounter) / (1.0 * SDL_GetPerformanceFrequency()));
    g_Time = currentPerfCounter / (1.0 * counterFrequency);
    
    // Copy old input state to new input state
    *newGameInput = {};
    GameControllerInput* oldController1 = &oldGameInput->controllers[0];
    GameControllerInput* newController1 = &newGameInput->controllers[0];
    for(rgInt buttonIdx = 0; buttonIdx < rgArrayCount(GameControllerInput::buttons); ++buttonIdx)
    {
        newController1->buttons[buttonIdx].endedDown = oldController1->buttons[buttonIdx].endedDown;
    }
    
    // Copy old mouse input state to new mouse input state
    newGameInput->mouse.x = oldGameInput->mouse.x;
    newGameInput->mouse.y = oldGameInput->mouse.y;
    for(rgInt buttonIdx = 0; buttonIdx < rgArrayCount(GameControllerInput::buttons); ++buttonIdx)
    {
        newGameInput->mouse.buttons[buttonIdx].endedDown = oldGameInput->mouse.buttons[buttonIdx].endedDown;
    }
    
    g_GameInput = newGameInput;
    
    SDL_Event event;
    while(SDL_PollEvent(&event) != 0)
    {
        ImGui_ImplSDL2_ProcessEvent(&event); // TODO: Is this the right to place this here?
        
        if(event.type == SDL_QUIT)
        {
            g_ShouldAppQuit = true;
        }
        else if(event.type == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            g_WindowInfo.width = event.window.data1;
            g_WindowInfo.height = event.window.data2;
            gfxOnSizeChanged();
        }
        else
        {
            // process event
            processGameInputs(&event, newGameInput);
        }
    }
    
    gfxStartNextFrame();
    gfxRunOnFrameBeginJob();
    
    gfxRendererImGuiNewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void TheApp::afterUpdateAndDraw()
{
    ImGui::Render();
    gfxRendererImGuiRenderDrawData();
    gfxEndFrame();
    
    *oldGameInput = *newGameInput;
}

void TheApp::endApp()
{
    
}

void TheApp::setTitle(const char *_title)
{
}
#endif

// FILE IO
// -------

FileData fileRead(const char* filepath)
{
	FileData result = {};

	SDL_RWops* fp = SDL_RWFromFile(filepath, "rb");

	if (fp != NULL)
	{
		SDL_RWseek(fp, 0, RW_SEEK_END);
		Sint64 size = (rgSize)SDL_RWtell(fp);

		if (size == -1 || size <= 0)
		{
			SDL_RWclose(fp);
			result.isValid = false;
			return result;
		}
		else
		{
			result.dataSize = (rgSize)size;
		}

		SDL_RWseek(fp, 0, RW_SEEK_SET);

		result.data = (rgU8*)rgMalloc(sizeof(rgU8) * size);
		if (result.data == NULL)
		{
			rgLog("Cannot allocate memory(%dbytes) for reading file %s", size, filepath);
			SDL_RWclose(fp);
			result.isValid = false;
			return result;
		}

		rgSize sizeRead = SDL_RWread(fp, result.data, sizeof(rgU8), size);
		if (sizeRead != size)
		{
			result.isValid = false;
			rgFree(result.data);
		}

		SDL_RWclose(fp);

		result.isValid = true;
	}
	else
	{
		rgLog("Cannot open/read file %s", filepath);
		result.isValid = false;
	}

	return result;
}

rgBool fileWrite(char const* filepath, void* bufferPtr, rgSize bufferSizeInBytes)
{
	// Open file for writing
	SDL_RWops* fp = SDL_RWFromFile(filepath, "wb");

	// If file is opened
	if (fp != 0)
	{
		size_t bytesWritten = SDL_RWwrite(fp, bufferPtr, sizeof(rgU8), bufferSizeInBytes);
		if (bytesWritten != (size_t)bufferSizeInBytes)
		{
			rgLogError("Can't write file %s completely\n", filepath);
			return false;
		}

		SDL_RWclose(fp);
	}
	// If file can't be opened
	else
	{
		rgLogError("Can't open file %s for writing\n", filepath);
		return false;
	}
	return true;
}

void fileFree(FileData* fd)
{
    rgFree(fd->data);
}

// UTILS
// -----

void engineLogfImpl(char const* fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_TEST, SDL_LOG_PRIORITY_DEBUG, fmt, argList);
    va_end(argList);
}

double getTime()
{
    return g_Time;
}

double getDeltaTime()
{
    return g_DeltaTime;
}

char* getSaveDataPath()
{
    return SDL_GetPrefPath("rg", "gamelib");
}
