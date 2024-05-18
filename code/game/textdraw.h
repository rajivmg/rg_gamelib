#ifndef __TEXTDRAW_H__
#define __TEXTDRAW_H__

#include "core.h"
#include "gfx.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/hash_map.h>

struct Glyph
{
    unsigned id;
    unsigned width;
    unsigned height;
    int xOffset;
    int yOffset;
    int xAdvance;
    QuadUV uv;
};

struct Font
{
    unsigned lineHeight;
    unsigned base;
    
    GfxTexture* texture;
    
    unsigned glyphCount;
    eastl::hash_map<unsigned, Glyph> glyphs;
};

typedef eastl::shared_ptr<Font> FontRef;

FontRef loadFont(char const* fontFilename);
void    unloadFont(Font* font);

void pushText(TexturedQuads* quadList, uint32 x, uint32 y, FontRef font, float fontSize, const char* text);

#endif
