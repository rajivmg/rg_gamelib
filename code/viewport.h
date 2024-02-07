#ifndef __VIEWPORT_H__
#define __VIEWPORT_H__

#include "core.h"

class Viewport
{
public:
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

	Viewport();
	~Viewport();

	void tick();

protected:
	void updateCamera();
};

#endif
