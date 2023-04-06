#include "../rg_gfx.h"

#include <box2d/box2d.h>

struct GameData
{
    rg::TexturedQuads characterPortraits;
    rg::TexturedQuads terrainAndOcean;
    rg::HGfxTexture2D oceanTileTexture;
    rg::HGfxTexture2D flowerTexture;
    
    rg::GfxGraphicsPSO* simple2dPSO; // TODO: use Handle

    b2World* phyWorld;
};
