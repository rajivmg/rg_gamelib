#ifndef __VIEWPORT_H__
#define __VIEWPORT_H__

#include "core.h"

struct CameraParamsGPU
{
    f32 basisMatrix[9];
    f32 _padding1[3];
    f32 viewMatrix[16];
    f32 projMatrix[16];
    f32 viewProjMatrix[16];
    f32 invViewMatrix[16];
    f32 invProjMatrix[16];
    f32 viewRotOnlyMatrix[16];
    f32 nearPlane;
    f32 farPlane;
    f32 _padding2[2];
};

class Viewport
{
public:
    // Camera info
    Vector3    cameraRight;
    Vector3    cameraUp;
    Vector3    cameraForward;

    Vector3    cameraPosition;
    f32    cameraPitch;
    f32    cameraYaw;

    Matrix3    cameraBasis;
    f32    cameraNear;
    f32    cameraFar;
    Matrix4    cameraView;
    Matrix4    cameraProjection;
    Matrix4    cameraViewProjection;
    Matrix4    cameraInvView;
    Matrix4    cameraInvProjection;
    Matrix4    cameraViewRotOnly;
    
    CameraParamsGPU cameraParamsGPU;
    
    Viewport();
    ~Viewport();

    void tick();
    
    CameraParamsGPU* getCameraParamsGPU()
    {
        return &cameraParamsGPU;
    }

protected:
    void updateCamera();
};

#endif
