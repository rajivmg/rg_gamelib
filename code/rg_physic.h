#pragma once

#include "core.h"

static constexpr i32 kMaxParticles = 2048;

struct PhysicSystem
{
    // TODO: use pointers, then simply exchange pointers
    rgFloat3 particlePos[kMaxParticles];
    rgFloat3 particlePrevPos[kMaxParticles];
    rgFloat3 particleForceAccumulators[kMaxParticles];

    rgFloat3 gravity;
    f32  timestep;
};

void TickPhysicSystem(PhysicSystem* system);
