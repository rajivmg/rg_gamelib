#define RG_H_IMPLEMENTATION
#include "rg.h"

#include "rg_gfx.h"
#include "rg_physic.h"

#include "backends/imgui_impl_sdl2.h"

#include "box2d/box2d.h"

#include <EASTL/vector.h>
#include "game/game.h"

#include "shaders/metal/simple2d_shader.inl"
#include "shaders/metal/principledbrdf_shader.inl"

// TODO:
// 2. Add rgLogDebug() rgLogError()
// 3. Then use the suitable rgLogXXX() version in VKDbgReportCallback Function based on msgType 
// 4. Integrate EASTL
//

using namespace rg;

// Important: make this a pointer, otherwise if a type with constructor is added to struct, the compiler will complain because it will try to call the constructor of anonymous structs
//rg::GfxCtx* rg::g_GfxCtx;

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    //rgLog("new[] size=%ld alignment=%ld alignmentOffset=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
    //    size, alignment, alignmentOffset, pName, flags, debugFlags, file, line);
    return new uint8_t[size];
}

void* __cdecl operator new[](size_t size, const char* name, int flags, unsigned debugFlags, const char* file, int line)
{
    //rgLog("new[] size=%ld name=%s flags=%d debugFlags=%d file=%s line=%d\n",
    //    size, name, flags, debugFlags, file, line);
    return new uint8_t[size];
}

GameState* g_GameState;
GameInput* g_GameInput;
rg::PhysicSystem* g_PhysicSystem;
rg::WindowInfo g_WindowInfo;

rgDouble g_DeltaTime;
rgDouble g_Time;

rgInt g_FrameIndex;

rgBool g_ShouldQuit;

rgU32 rgRenderKey(rgBool top)
{
    return top ? 1 : 0;
}

void updateCamera()
{
    GameControllerInput* controller = &g_GameInput->controllers[0];
    rgAssert(controller);
    GameMouseState* mouse = &g_GameInput->mouse;
    rgAssert(mouse);
    
    const Vector3 worldNorth = Vector3(0.0f, 0.0f, 1.0f);
    const Vector3 worldEast = Vector3(1.0f, 0.0f, 0.0f);
    const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
    
    const rgDouble camMoveSpeed = 2.9;
    const rgDouble camStrafeSpeed = 3.6;
    // TODO: take FOV in account
    const rgDouble camHorizonalRotateSpeed = (M_PI / g_WindowInfo.width);
    const rgDouble camVerticalRotateSpeed = (M_PI / g_WindowInfo.height);

    // Position Delta
    const rgFloat forward = camMoveSpeed * ((controller->forward.endedDown ? g_DeltaTime : 0.0) + (controller->backward.endedDown ? -g_DeltaTime : 0.0));
    const rgFloat strafe = camStrafeSpeed * ((controller->right.endedDown ? g_DeltaTime : 0.0) + (controller->left.endedDown ? -g_DeltaTime : 0.0));
    const rgFloat ascent = camStrafeSpeed * ((controller->up.endedDown ? g_DeltaTime : 0.0) + (controller->down.endedDown ? -g_DeltaTime : 0.0));
    
    // Orientation Delta
    const rgFloat yaw = (mouse->relX * camHorizonalRotateSpeed) + g_GameState->cameraYaw;
    const rgFloat pitch = (mouse->relY * camVerticalRotateSpeed) + g_GameState->cameraPitch;
    
    // COMPUTE VIEW MATRIX
    // Apply Yaw
    const Quat yawQuat = Quat::rotation(yaw, worldUp);
    const Vector3 yawedTarget = normalize(rotate(yawQuat, worldEast));
    // Apply Pitch
    const Vector3 right = normalize(cross(worldUp, yawedTarget));
    const Quat pitchQuat = Quat::rotation(pitch, right);
    const Vector3 pitchedYawedTarget = normalize(rotate(pitchQuat, yawedTarget));
    
    // SET CAMERA STATE
    g_GameState->cameraRight = right;
    g_GameState->cameraUp = normalize(cross(pitchedYawedTarget, right));
    g_GameState->cameraForward = pitchedYawedTarget;

    g_GameState->cameraBasis = Matrix3(g_GameState->cameraRight, g_GameState->cameraUp, g_GameState->cameraForward);

    g_GameState->cameraPosition += g_GameState->cameraBasis * Vector3(strafe, ascent, -forward);
    g_GameState->cameraPitch = pitch;
    g_GameState->cameraYaw = yaw;
    
    g_GameState->cameraNear = 0.01f;
    g_GameState->cameraFar  = 1000.0f;
    
    g_GameState->cameraView = orthoInverse(Matrix4(g_GameState->cameraBasis, Vector3(g_GameState->cameraPosition)));
    g_GameState->cameraProjection = gfx::makePerspectiveProjectionMatrix(1.4f, g_WindowInfo.width/g_WindowInfo.height, g_GameState->cameraNear, g_GameState->cameraFar);
    g_GameState->cameraViewProjection = g_GameState->cameraProjection * g_GameState->cameraView;
    g_GameState->cameraInvView = inverse(g_GameState->cameraView);
    g_GameState->cameraInvProjection = inverse(g_GameState->cameraProjection);
}

rgInt rg::setup()
{
    g_GameState = rgNew(GameState);

    BitmapRef tinyTexture = loadBitmap("tiny.tga");
    gfx::texture2D->create("tiny", tinyTexture->buf, tinyTexture->width, tinyTexture->height, tinyTexture->format, false, GfxTextureUsage_ShaderRead);
    gfx::texture2D->destroy(rgCRC32("tiny"));
    
    for(rgInt i = 1; i <= 16; ++i)
    {
        char path[256];
        snprintf(path, 256, "debugTextures/textureSlice%d.png", i);
        BitmapRef t = loadBitmap(path);
        GfxTexture2D* t2d = gfx::texture2D->create(path, t->buf, t->width, t->height, t->format, false, GfxTextureUsage_ShaderRead);
        gfx::debugTextureHandles.push_back(t2d);
    }

    BitmapRef flowerTex = rg::loadBitmap("flower.png");
    g_GameState->flowerTexture = gfx::texture2D->create("flower", flowerTex->buf, flowerTex->width, flowerTex->height, flowerTex->format, false, GfxTextureUsage_ShaderRead);
    
    //gfxDestroyBuffer("ocean_tile");
    g_GameState->shaderballModel = rg::loadModel("shaderball.xml");
    
    GfxVertexInputDesc vertexDesc = {};
    vertexDesc.elementCount = 3;
    vertexDesc.elements[0].semanticName = "POSITION";
    vertexDesc.elements[0].semanticIndex = 0;
    vertexDesc.elements[0].offset = 0;
    vertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexDesc.elements[0].bufferIndex = 0; // TODO: rename to slot
    vertexDesc.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexDesc.elements[1].semanticName = "TEXCOORD";
    vertexDesc.elements[1].semanticIndex = 0;
    vertexDesc.elements[1].offset = 12;
    vertexDesc.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
    vertexDesc.elements[1].bufferIndex = 0; // TODO: Automate this.. MTL: 30 - bufferIndex = vb bindpoint, to maintain compat with DX12 way
    vertexDesc.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexDesc.elements[2].semanticName = "COLOR";
    vertexDesc.elements[2].semanticIndex = 0;
    vertexDesc.elements[2].offset = 20;
    vertexDesc.elements[2].format = TinyImageFormat_R32G32B32A32_SFLOAT;
    vertexDesc.elements[2].bufferIndex = 0;
    vertexDesc.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrc = "simple2d.hlsl";
    simple2dShaderDesc.vsEntrypoint = "vsSimple2d";
    simple2dShaderDesc.fsEntrypoint = "fsSimple2d";
    simple2dShaderDesc.defines = "RIGHT";
    
    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    //simple2dRenderStateDesc.triangleFillMode = GfxTriangleFillMode_Lines;
    
    gfx::graphicsPSO->create("simple2d", &vertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    
    //
    GfxVertexInputDesc vertexPosTexCoordNormal = {};
    vertexPosTexCoordNormal.elementCount = 3;
    vertexPosTexCoordNormal.elements[0].semanticName = "POSITION";
    vertexPosTexCoordNormal.elements[0].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[0].offset = 0;
    vertexPosTexCoordNormal.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexPosTexCoordNormal.elements[0].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[0].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexPosTexCoordNormal.elements[1].semanticName = "TEXCOORD";
    vertexPosTexCoordNormal.elements[1].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[1].offset = 12;
    vertexPosTexCoordNormal.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
    vertexPosTexCoordNormal.elements[1].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[1].stepFunc = GfxVertexStepFunc_PerVertex;
    
    vertexPosTexCoordNormal.elements[2].semanticName = "NORMAL";
    vertexPosTexCoordNormal.elements[2].semanticIndex = 0;
    vertexPosTexCoordNormal.elements[2].offset = 20;
    vertexPosTexCoordNormal.elements[2].format = TinyImageFormat_R32G32B32_SFLOAT;
    vertexPosTexCoordNormal.elements[2].bufferIndex = 0;
    vertexPosTexCoordNormal.elements[2].stepFunc = GfxVertexStepFunc_PerVertex;

    GfxShaderDesc principledBrdfShaderDesc = {};
    principledBrdfShaderDesc.shaderSrc = "pbr.hlsl";
    principledBrdfShaderDesc.vsEntrypoint = "vsPbr";
    principledBrdfShaderDesc.fsEntrypoint = "fsPbr";
    principledBrdfShaderDesc.defines = "LEFT";
    
    GfxRenderStateDesc world3dRenderState = {};
    world3dRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    world3dRenderState.colorAttachments[0].blendingEnabled = true;
    world3dRenderState.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    world3dRenderState.depthWriteEnabled = true;
    world3dRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    world3dRenderState.winding = GfxWinding_CCW;
    world3dRenderState.cullMode = GfxCullMode_None;

    gfx::graphicsPSO->create("principledBrdf", &vertexPosTexCoordNormal, &principledBrdfShaderDesc, &world3dRenderState);
    //
    GfxShaderDesc gridShaderDesc = {};
    gridShaderDesc.shaderSrc = "grid.hlsl";
    gridShaderDesc.vsEntrypoint = "vsGrid";
    gridShaderDesc.fsEntrypoint = "fsGrid";
    
    GfxRenderStateDesc gridRenderState = {};
    gridRenderState.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    gridRenderState.colorAttachments[0].blendingEnabled = true;
    gridRenderState.depthStencilAttachmentFormat = TinyImageFormat_D16_UNORM;
    gridRenderState.depthWriteEnabled = true;
    gridRenderState.depthCompareFunc = GfxCompareFunc_LessEqual;
    gridRenderState.winding = GfxWinding_CCW;
    gridRenderState.cullMode = GfxCullMode_None;
    
    gfx::graphicsPSO->create("gridPSO", nullptr, &gridShaderDesc, &gridRenderState);
    //
    

    gfx::samplerState->create("nearestRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, true);
    
    // Initialize camera params
    g_GameState->cameraPosition = Vector3(0.0f, 3.0f, 3.0f);
    g_GameState->cameraPitch = 0.0f;
    g_GameState->cameraYaw = 0.0f;
    
    //updateCamera();
    
    g_PhysicSystem = rgNew(PhysicSystem);

#if 0
    b2Vec2 gravity(0.0f, -9.8f);
    g_GameData->phyWorld = rgNew(b2World)(gravity);
    b2World& world = *g_GameData->phyWorld;

    b2BodyDef groundBodyDef;
    groundBodyDef.position.Set(0.0f, -10.0f);
    b2Body* groundBody = world.CreateBody(&groundBodyDef);

    b2PolygonShape groundBox;
    groundBox.SetAsBox(50.0f, 10.0f);
    groundBody->CreateFixture(&groundBox, 0.0f);

    b2BodyDef boxBodyDef;
    boxBodyDef.type = b2_dynamicBody;
    boxBodyDef.position.Set(0.0f, 4.0f);
    b2Body* boxBody = world.CreateBody(&boxBodyDef);

    b2PolygonShape boxShape;
    boxShape.SetAsBox(1.0f, 1.0f);
    b2FixtureDef boxFixtureDef;
    boxFixtureDef.shape = &boxShape;
    boxFixtureDef.density = 1.0f;
    boxFixtureDef.friction = 0.3f;
    boxBody->CreateFixture(&boxFixtureDef);
#endif

    return 0;
}

rgFloat sgn(rgFloat x)
{
    return (x > 0) - (x < 0);
}

rgInt rg::updateAndDraw(rgDouble dt)
{
    //rgLog("DeltaTime:%f FPS:%.1f\n", dt, 1.0/dt);
    ImGui::ShowDemoWindow();
    
    if(g_GameInput->mouse.right.endedDown)
    {
        updateCamera();
    }
    
    // PREPARE COMMON RESOURCES & DATA
    struct CommonParams
    {
        rgFloat cameraBasisMatrix[9];
        rgFloat _padding1[3];
        //rgFloat cameraNear;
        //rgFloat cameraFar;
        //rgFloat _padding2[2];
        rgFloat cameraViewMatrix[16];
        rgFloat cameraProjMatrix[16];
        rgFloat cameraViewProjMatrix[16];
        rgFloat cameraInvViewMatrix[16];
        rgFloat cameraInvProjMatrix[16];
    } commonParams;
    
    copyMatrix3ToFloatArray(commonParams.cameraBasisMatrix, g_GameState->cameraBasis);
    //commonParams.cameraNear = g_GameState->cameraNear;
    //commonParams.cameraFar  = g_GameState->cameraFar;
    copyMatrix4ToFloatArray(commonParams.cameraViewMatrix, g_GameState->cameraView);
    copyMatrix4ToFloatArray(commonParams.cameraProjMatrix, g_GameState->cameraProjection);
    copyMatrix4ToFloatArray(commonParams.cameraViewProjMatrix, g_GameState->cameraViewProjection);
    copyMatrix4ToFloatArray(commonParams.cameraInvViewMatrix, g_GameState->cameraInvView);
    copyMatrix4ToFloatArray(commonParams.cameraInvProjMatrix, g_GameState->cameraInvProjection);

    GfxFrameResource cameraParamsBuffer = gfx::getFrameAllocator()->newBuffer("commonParams", sizeof(commonParams), &commonParams);
    
    // RENDER SIMPLE 2D STUFF
    {
        g_GameState->characterPortraits.resize(0);
        for(rgInt i = 0; i < 4; ++i)
        {
            for(rgInt j = 0; j < 4; ++j)
            {
                rgFloat px = j * (100) + 10 * (j + 1) + sinf((rgFloat)g_Time) * 30;
                rgFloat py = i * (100) + 10 * (i + 1) + cosf((rgFloat)g_Time) * 30;
                
                pushTexturedQuad(&g_GameState->characterPortraits, defaultQuadUV, {px, py, 100.0f, 100.0f}, {0, 0, 0, 0}, gfx::debugTextureHandles[j + i * 4]);
            }
        }
        pushTexturedQuad(&g_GameState->characterPortraits, defaultQuadUV, {200.0f, 300.0f, 447.0f, 400.0f}, {0, 0, 0, 0}, g_GameState->flowerTexture);
        
        GfxRenderPass simple2dRenderPass = {};
        simple2dRenderPass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
        simple2dRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Clear;
        simple2dRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        simple2dRenderPass.colorAttachments[0].clearColor = { 0.5f, 0.5f, 0.5f, 1.0f };
        simple2dRenderPass.depthStencilAttachmentTexture = gfx::depthStencilBuffer;
        simple2dRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Clear;
        simple2dRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        simple2dRenderPass.clearDepth = 1.0f;
        
        GfxRenderCmdEncoder* simple2dRenderEncoder = gfx::setRenderPass(&simple2dRenderPass, "Simple2D Pass");
        simple2dRenderEncoder->setGraphicsPSO(gfx::graphicsPSO->find("simple2d"_rh));
        simple2dRenderEncoder->drawTexturedQuads(&g_GameState->characterPortraits);
        simple2dRenderEncoder->end();
    }
        
    {
        GfxRenderPass demoScenePass = {};
        demoScenePass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
        demoScenePass.colorAttachments[0].loadAction = GfxLoadAction_Load;
        demoScenePass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        demoScenePass.depthStencilAttachmentTexture = gfx::getRenderTargetDepthBuffer();
        demoScenePass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        demoScenePass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* demoSceneEncoder = gfx::setRenderPass(&demoScenePass, "DemoScene Pass");
        demoSceneEncoder->setGraphicsPSO(gfx::graphicsPSO->find("principledBrdf"_rh));
        
        // instance
        struct
        {
            rgFloat worldXform[256][16];
            rgFloat invTposeWorldXform[256][16];
        } instanceParams;
        
        Matrix4 xform = Matrix4::identity();//Matrix4(Transform3::rotationY(sinf(g_Time * 0.5f)));
        copyMatrix4ToFloatArray(&instanceParams.worldXform[0][0], xform);
        copyMatrix4ToFloatArray(&instanceParams.invTposeWorldXform[0][0], transpose(inverse(xform)));
        
        
        GfxFrameResource instanceParamsBuffer = gfx::getFrameAllocator()->newBuffer("instanceParamsCBufferBunny", sizeof(instanceParams), &instanceParams);
        
        demoSceneEncoder->bindBuffer(&cameraParamsBuffer, "commonParams");
        demoSceneEncoder->bindBuffer(&instanceParamsBuffer, "instanceParams");
        
        for(rgInt i = 0; i < g_GameState->shaderballModel->meshes.size(); ++i)
        {
            rg::Mesh* m = &g_GameState->shaderballModel->meshes[i];
            
            demoSceneEncoder->setVertexBuffer(g_GameState->shaderballModel->vertexIndexBuffer, g_GameState->shaderballModel->vertexBufferOffset +  m->vertexDataOffset, 0);
            demoSceneEncoder->drawIndexedTriangles(m->indexCount, true, g_GameState->shaderballModel->vertexIndexBuffer, g_GameState->shaderballModel->index32BufferOffset + m->indexDataOffset, 1);
        }

        demoSceneEncoder->end();
    }
    
    // RENDER GRID AND EDITOR STUFF
    {
        GfxRenderPass gridRenderPass = {};
        gridRenderPass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
        gridRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Load;
        gridRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
        gridRenderPass.depthStencilAttachmentTexture = gfx::getRenderTargetDepthBuffer();
        gridRenderPass.depthStencilAttachmentLoadAction = GfxLoadAction_Load;
        gridRenderPass.depthStencilAttachmentStoreAction = GfxStoreAction_Store;
        
        GfxRenderCmdEncoder* gridRenderEncoder = gfx::setRenderPass(&gridRenderPass, "DemoScene Pass");
        gridRenderEncoder->setGraphicsPSO(gfx::graphicsPSO->find("gridPSO"_tag));
        gridRenderEncoder->bindBuffer(&cameraParamsBuffer, "commonParams");
        gridRenderEncoder->drawTriangles(0, 6, 1);
        gridRenderEncoder->end();
    }
    
    rgHash a = rgCRC32("hello world");

#if 0
    rgFloat timeStep = 1.0f / 60.0f;
    int32 velocityIterations = 6;
    int32 positionIterations = 2;

    g_GameData->phyWorld->Step(timeStep, velocityIterations, positionIterations);
    g_GameData->phyWorld->Dump();
#endif

    return 0;
}

static rgBool ProcessGameButtonState(GameButtonState* newButtonState, rgBool isDown)
{
    if(newButtonState->endedDown != isDown)
    {
        newButtonState->endedDown = isDown;
        ++newButtonState->halfTransitionCount;
    }
}

rgBool ProcessGameInputs(SDL_Event* event, GameInput* gameInput)
{
    GameControllerInput* controller1 = &gameInput->controllers[0];
    
    switch(event->type)
    {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            if(!event->key.repeat)
            {
                rgBool isDown = (event->key.state == SDL_PRESSED);
                switch(event->key.keysym.sym)
                {
                    case SDLK_w:
                    case SDLK_UP:
                    {
                        ProcessGameButtonState(&controller1->forward, isDown);
                    } break;
                        
                    case SDLK_s:
                    case SDLK_DOWN:
                    {
                        ProcessGameButtonState(&controller1->backward, isDown);
                    } break;
                        
                    case SDLK_a:
                    case SDLK_LEFT:
                    {
                        ProcessGameButtonState(&controller1->left, isDown);
                    } break;
                        
                    case SDLK_d:
                    case SDLK_RIGHT:
                    {
                        ProcessGameButtonState(&controller1->right, isDown);
                    } break;
                    
                    case SDLK_q:
                    case SDLK_c:
                    {
                        ProcessGameButtonState(&controller1->up, isDown);
                    } break;
                        
                    case SDLK_e:
                    case SDLK_f:
                    {
                        ProcessGameButtonState(&controller1->down, isDown);
                    } break;
                    
                    case SDLK_ESCAPE:
                    {
                        SDL_Event quitEvent;
                        quitEvent.type = SDL_QUIT;
                        SDL_PushEvent(&quitEvent);
                    } break;
                }
            }
        } break;
            
        case SDL_MOUSEMOTION:
        {
            gameInput->mouse.x = event->motion.x;
            gameInput->mouse.y = event->motion.y;
            gameInput->mouse.relX = event->motion.xrel;
            gameInput->mouse.relY = event->motion.yrel;
        } break;
            
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
        {
            rgBool isDown = (event->button.state == SDL_PRESSED);
            switch(event->button.button)
            {
                case SDL_BUTTON_LEFT:
                {
                    ProcessGameButtonState(&gameInput->mouse.left, isDown);
                } break;
                    
                case SDL_BUTTON_MIDDLE:
                {
                    ProcessGameButtonState(&gameInput->mouse.middle, isDown);
                } break;
                    
                case SDL_BUTTON_RIGHT:
                {
                    ProcessGameButtonState(&gameInput->mouse.right, isDown);
                } break;
            }
        } break;
    }
}

rgInt createSDLWindow()
{
    g_WindowInfo.width = 1056.0f;
    g_WindowInfo.height = 594.0f;

    Uint32 windowFlags = 0;
    
#if defined(RG_VULKAN_RNDR)
    windowFlags |= SDL_WINDOW_VULKAN;
#elif defined(RG_METAL_RNDR)
    windowFlags |= SDL_WINDOW_ALWAYS_ON_TOP;
    windowFlags |= SDL_WINDOW_METAL;
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#elif defined(RG_OPENGL_RNDR)
    windowFlags |= SDL_WINDOW_OPENGL;
#elif defined(RG_D3D12_RNDR)
    windowFlags |= SDL_WINDOW_RESIZABLE;
#endif
    
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
    {
#if defined(RG_OPENGL_RNDR)
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 2);
#endif
        gfx::mainWindow = SDL_CreateWindow("gamelib",
                                           SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                           g_WindowInfo.width, g_WindowInfo.height, windowFlags);
        rgAssert(gfx::mainWindow != NULL);
        return 0;
    }
    
    return -1;
}

int main(int argc, char* argv[])
{
    g_FrameIndex = -1;

    if(createSDLWindow() != 0)
    {
        return -1; // error;
    }

    rgInt gfxPreInitResult = gfx::preInit();
    rgInt gfxInitResult = gfx::init();
    rgInt gfxCommonInitResult = gfx::initCommonStuff();

    if(gfxInitResult || gfxCommonInitResult)
    {
        return gfxInitResult | gfxCommonInitResult;
    }

    rg::setup();

    g_ShouldQuit = false;
    SDL_Event event;
    
    Uint64 currentPerfCounter = SDL_GetPerformanceCounter();
    Uint64 previousPerfCounter = currentPerfCounter;
    
    GameInput inputs[2] = {};
    GameInput* oldGameInput = &inputs[0];
    GameInput* newGameInput = &inputs[1];
    
    while(!g_ShouldQuit)
    {
        ++gfx::frameNumber;

        Uint64 counterFrequency = SDL_GetPerformanceFrequency();
        previousPerfCounter = currentPerfCounter;
        currentPerfCounter = SDL_GetPerformanceCounter();
        g_DeltaTime = ((currentPerfCounter - previousPerfCounter) / (1.0 * SDL_GetPerformanceFrequency()));
        g_Time = currentPerfCounter / (1.0 * counterFrequency);
        
        // Copy old input state to new input state
        *newGameInput = {};
        GameControllerInput* oldController1 = &oldGameInput->controllers[0];
        GameControllerInput* newController1 = &newGameInput->controllers[0];
        for(rgInt buttonIdx = 0; buttonIdx < rgARRAY_COUNT(GameControllerInput::buttons); ++buttonIdx)
        {
            newController1->buttons[buttonIdx].endedDown = oldController1->buttons[buttonIdx].endedDown;
        }
        
        // Copy old mouse input state to new mouse input state
        newGameInput->mouse.x = oldGameInput->mouse.x;
        newGameInput->mouse.y = oldGameInput->mouse.y;
        for(rgInt buttonIdx = 0; buttonIdx < rgARRAY_COUNT(GameControllerInput::buttons); ++buttonIdx)
        {
            newGameInput->mouse.buttons[buttonIdx].endedDown = oldGameInput->mouse.buttons[buttonIdx].endedDown;
        }
        
        g_GameInput = newGameInput;
        
        while(SDL_PollEvent(&event) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&event); // TODO: Is this the right to place this here?
            
            if(event.type == SDL_QUIT)
            {
                g_ShouldQuit = true;
            }
            else if(event.type == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                g_WindowInfo.width = event.window.data1;
                g_WindowInfo.height = event.window.data2;
                gfx::onSizeChanged();
            }
            else
            {
                // process event
                ProcessGameInputs(&event, newGameInput);
            }
        }
        
        gfx::startNextFrame();
        
        gfx::rendererImGuiNewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        rg::updateAndDraw(g_DeltaTime);
         
        ImGui::Render();
        gfx::rendererImGuiRenderDrawData();
        gfx::endFrame();
        
        *oldGameInput = *newGameInput;
    }

    gfx::destroy();
    SDL_DestroyWindow(gfx::mainWindow);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    return 0;
}
