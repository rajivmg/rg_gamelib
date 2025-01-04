#ifndef __VIEWPORT_H__
#define __VIEWPORT_H__

#include "core.h"

struct CameraParamsGPU
{
    rgFloat basisMatrix[9];
    rgFloat _padding1[3];
    rgFloat viewMatrix[16];
    rgFloat projMatrix[16];
    rgFloat viewProjMatrix[16];
    rgFloat invViewMatrix[16];
    rgFloat invProjMatrix[16];
    rgFloat viewRotOnlyMatrix[16];
    rgFloat near;
    rgFloat far;
    rgFloat _padding2[2];
};

class Viewport
{
public:
    // Camera info
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
