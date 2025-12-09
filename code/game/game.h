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
    
    
    f32 tonemapperMinLogLuminance;
    f32 tonemapperMaxLogLuminance;
    f32 tonemapperAdaptationRate;
    f32 tonemapperExposureKey;
    
    rgBool  debugShowGrid;
    
    GfxTexture* baseColor2DRT;
    GfxTexture* baseColorRT;
    GfxTexture* depthStencilRT;

    b2World* phyWorld;
};
