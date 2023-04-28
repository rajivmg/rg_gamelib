#include "../rg_gfx.h"

#include <box2d/box2d.h>

using namespace rg;
//using namespace gfx;

struct GameData
{
    gfx::TexturedQuads characterPortraits;
    gfx::TexturedQuads terrainAndOcean;
    gfx::GfxTexture2D* oceanTileTexture;
    gfx::GfxTexture2D* flowerTexture;
    
    gfx::GfxGraphicsPSO* simple2dPSO; // TODO: use Handle

    b2World* phyWorld;
};
