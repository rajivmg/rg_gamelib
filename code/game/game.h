#include "../rg_gfx.h"

#include <box2d/box2d.h>

struct GameData
{
    rg::TexturedQuads characterPortraits;
    rg::TexturedQuads terrainAndOcean;
    rg::GfxTexture2DHandle oceanTileTexture;
    rg::GfxTexture2DHandle flowerTexture;

    b2World* phyWorld;
};
