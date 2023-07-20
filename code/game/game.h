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
            GameButtonState jump;
        };
    };
};

extern GameControllerInput* g_GameControllerInputs[4];

struct GameState
{
    TexturedQuads characterPortraits;
    TexturedQuads terrainAndOcean;
    GfxTexture2D* oceanTileTexture;
    GfxTexture2D* flowerTexture;

    ModelRef shaderballModel;
    
    GfxGraphicsPSO* simple2dPSO;
    
    Vector3    cameraPos;
    Vector3    cameraRot;
    Matrix3 cameraMat3;
    

    b2World* phyWorld;
};
