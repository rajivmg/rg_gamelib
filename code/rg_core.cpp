#include "rg_core.h"

RG_BEGIN_RG_NAMESPACE

char* getPrefPath()
{
    return SDL_GetPrefPath("rg", "gamelib");
}

RG_END_RG_NAMESPACE
