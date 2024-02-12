#include "viewport.h"

#include "gfx.h"

Viewport::Viewport()
{
	cameraPosition = Vector3(0.0f, 3.0f, -3.0f);
	cameraPitch = ((rgFloat)M_PI / 4.0f) - 0.15f; // ~45 deg
	cameraYaw = (rgFloat)M_PI / -2.0f; // 90deg
}

Viewport::~Viewport()
{

}

void Viewport::tick()
{
	if (theAppInput->mouse.right.endedDown)
	{
		updateCamera();
	}
}

void Viewport::updateCamera()
{
	GameControllerInput* controller = &theAppInput->controllers[0];
	rgAssert(controller);
	GameMouseState* mouse = &theAppInput->mouse;
	rgAssert(mouse);

	const float dT = (float)theAppInput->deltaTime;

	const Vector3 worldNorth = Vector3(0.0f, 0.0f, 1.0f);
	const Vector3 worldEast = Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);

	const rgFloat camMoveSpeed = 2.9f;
	const rgFloat camStrafeSpeed = 3.6f;
	// TODO: take FOV in account
	const rgFloat camHorizonalRotateSpeed = (rgFloat)M_PI / g_WindowInfo.width;
	const rgFloat camVerticalRotateSpeed = (rgFloat)M_PI / g_WindowInfo.height;

	// Position Delta
	const rgFloat forward = (rgFloat)(camMoveSpeed * ((controller->forward.endedDown ? dT : 0.0) + (controller->backward.endedDown ? -dT : 0.0)));
	const rgFloat strafe = (rgFloat)(camStrafeSpeed * ((controller->right.endedDown ? dT : 0.0) + (controller->left.endedDown ? -dT : 0.0)));
	const rgFloat ascent = (rgFloat)(camStrafeSpeed * ((controller->up.endedDown ? dT : 0.0) + (controller->down.endedDown ? -dT : 0.0)));

	// Orientation Delta
	const rgFloat yaw = (-mouse->relX * camHorizonalRotateSpeed) + cameraYaw;
	const rgFloat pitch = (-mouse->relY * camVerticalRotateSpeed) + cameraPitch;

	// COMPUTE VIEW MATRIX
	// Apply Yaw
	const Quat yawQuat = Quat::rotation(yaw, worldUp);
	const Vector3 yawedTarget = normalize(rotate(yawQuat, worldEast));
	// Apply Pitch
	const Vector3 right = normalize(cross(worldUp, yawedTarget));
	const Quat pitchQuat = Quat::rotation(pitch, right);
	const Vector3 pitchedYawedTarget = normalize(rotate(pitchQuat, yawedTarget));

	// SET CAMERA STATE
	cameraRight = right;
	cameraUp = normalize(cross(pitchedYawedTarget, right));
	cameraForward = pitchedYawedTarget;

	cameraBasis = Matrix3(cameraRight, cameraUp, cameraForward);

	cameraPosition += cameraBasis * Vector3(strafe, ascent, forward);
	cameraPitch = pitch;
	cameraYaw = yaw;

	cameraNear = 0.01f;
	cameraFar = 100.0f;

	cameraView = orthoInverse(Matrix4(cameraBasis, Vector3(cameraPosition)));
	cameraProjection = makePerspectiveProjectionMatrix(1.4f, (rgFloat)g_WindowInfo.width / g_WindowInfo.height, cameraNear, cameraFar);
	cameraViewProjection = cameraProjection * cameraView;
	cameraInvView = inverse(cameraView);
	cameraInvProjection = inverse(cameraProjection);

	cameraViewRotOnly = Matrix4(cameraView.getUpper3x3(), Vector3(0, 0, 0));
}