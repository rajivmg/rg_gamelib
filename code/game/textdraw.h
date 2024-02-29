#ifndef __TEXTDRAW_H__
#define __TEXTDRAW_H__

#include "core.h"
#include <EASTL/shared_ptr.h>

struct Font
{
    
};

typedef eastl::shared_ptr<Font> FontRef;

FontRef loadFont(char const* fontFilename);
void    unloadFont(Font* font);

#endif
