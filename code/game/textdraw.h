#ifndef __TEXTDRAW_H__
#define __TEXTDRAW_H__

#include "core.h"
#include "gfx.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/hash_map.h>

struct Glyph
{
    unsigned id;
    int xOffset;
    int yOffset;
    int xAdvance;
    QuadUV uv;
};

struct Font
{
    unsigned lineHeight;
    unsigned base;
    
    // Remove this
    unsigned scaleW;
    unsigned scaleH;
    //
    
    GfxTexture* atlasTexture;
    
    unsigned glyphCount;
    //eastl::vector<Glyph> glyphs;
    eastl::hash_map<unsigned, Glyph> glyphs;
};

typedef eastl::shared_ptr<Font> FontRef;

FontRef loadFont(char const* fontFilename);
void    unloadFont(Font* font);

#endif
