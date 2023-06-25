#pragma once

#include "rg.h"

RG_BEGIN_RG_NAMESPACE

static constexpr rgInt kMaxParticles = 2048;

struct PhysicSystem
{
    // TODO: use pointers, then simply exchange pointers
    rgFloat3 particlePos[kMaxParticles];
    rgFloat3 particlePrevPos[kMaxParticles];
    rgFloat3 particleForceAccumulators[kMaxParticles];

    rgFloat3 gravity;
    rgFloat  timestep;
};

void TickPhysicSystem(PhysicSystem* system);

RG_END_RG_NAMESPACE