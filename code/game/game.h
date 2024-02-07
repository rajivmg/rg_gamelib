#include "../gfx.h"

#include <box2d/box2d.h>

struct GameState
{
    TexturedQuads characterPortraits;
    TexturedQuads terrainAndOcean;
    GfxTexture* oceanTileTexture;
    GfxTexture* flowerTexture;

    ModelRef shaderballModel;
    
    GfxGraphicsPSO* simple2dPSO;
    
    
    rgFloat tonemapperMinLogLuminance;
    rgFloat tonemapperMaxLogLuminance;
    rgFloat tonemapperAdaptationRate;
    rgFloat tonemapperExposureKey;
    
    rgBool  debugShowGrid;
    
    GfxTexture* baseColor2DRT;
    GfxTexture* baseColorRT;
    GfxTexture* depthStencilRT;

    b2World* phyWorld;
};
