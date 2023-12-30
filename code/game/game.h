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
    
    // part of in-game editor state
    Vector3    cameraRight;
    Vector3    cameraUp;
    Vector3    cameraForward;
    
    Vector3    cameraPosition;
    rgFloat    cameraPitch;
    rgFloat    cameraYaw;
    
    Matrix3    cameraBasis;
    rgFloat    cameraNear;
    rgFloat    cameraFar;
    Matrix4    cameraView;
    Matrix4    cameraProjection;
    Matrix4    cameraViewProjection;
    Matrix4    cameraInvView;
    Matrix4    cameraInvProjection;
    Matrix4    cameraViewRotOnly;

    
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
