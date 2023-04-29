#include "../rg_gfx.h"

#include <box2d/box2d.h>

using namespace rg;
//using namespace gfx;

struct GameData
{
    TexturedQuads characterPortraits;
    TexturedQuads terrainAndOcean;
    GfxTexture2D* oceanTileTexture;
    GfxTexture2D* flowerTexture;
    
    GfxGraphicsPSO* simple2dPSO; // TODO: use Handle

    b2World* phyWorld;
};
