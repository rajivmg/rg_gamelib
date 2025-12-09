#include "rg_physic.h"

static void Verlet(PhysicSystem* sys)
{
    for(u32 i = 0; i < kMaxParticles; ++i)
    {
        rgFloat3& curPos = sys->particlePos[i];
        rgFloat3& prevPos = sys->particlePrevPos[i];
        rgFloat3& forceAcc = sys->particleForceAccumulators[i];
        rgFloat3 temp = curPos;

        curPos += curPos - prevPos + forceAcc * (sys->timestep * sys->timestep);

        prevPos = temp;
    }
}

static void SatisfyConstraints(PhysicSystem* system)
{

}

static void AccumulateForces(PhysicSystem* system)
{
    for(u32 i = 0; i < kMaxParticles; ++i)
    {
        system->particleForceAccumulators[i] = system->gravity;
    }
}

void TickPhysicSystem(PhysicSystem* system)
{
    AccumulateForces(system);
    Verlet(system);
    SatisfyConstraints(system);
}
