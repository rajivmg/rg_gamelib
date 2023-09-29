#include "../rg_gfx.h"

#include <box2d/box2d.h>

using namespace rg;
//using namespace gfx;

struct GameButtonState
{
    rgBool endedDown;
    rgUInt halfTransitionCount;
};

struct GameMouseState
{
    rgS32 x, y, relX, relY;
    
    union
    {
        GameButtonState buttons[3];
        struct
        {
            GameButtonState left;
            GameButtonState middle;
            GameButtonState right;
        };
    };
};

struct GameControllerInput
{
    union
    {
        GameButtonState buttons[5];
        struct
        {
            GameButtonState forward;
            GameButtonState backward;
            GameButtonState left;
            GameButtonState right;
            GameButtonState up;
            GameButtonState down;
            GameButtonState jump;
        };
    };
};

#define RG_MAX_GAME_CONTROLLERS 4

struct GameInput
{
    GameMouseState mouse;
    GameControllerInput controllers[RG_MAX_GAME_CONTROLLERS];
};

extern GameInput* g_GameInput;


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
    
    GfxTexture* baseColorRT;
    GfxTexture* depthStencilRT;

    b2World* phyWorld;
};
