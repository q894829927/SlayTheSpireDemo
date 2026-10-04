#include "InteriorPortalChaosSpeedCap.h"

#include "Chaos/ChaosEngineInterface.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "PhysicsEngine/BodyInstance.h"

namespace InteriorPortalPhysics
{
	bool FChaosTravellerSpeedCap::Install_GameThread(FBodyInstance& Body, float MaxSpeedCmPerSecond)
	{
		if (!IsInGameThread() || !FMath::IsFinite(MaxSpeedCmPerSecond) || MaxSpeedCmPerSecond <= 0
			|| static_cast<double>(MaxSpeedCmPerSecond)*MaxSpeedCmPerSecond >= TNumericLimits<float>::Max()
			|| !Body.IsInstanceSimulatingPhysics()) { return false; }
		const FPhysicsActorHandle Actor = Body.GetPhysicsActor();
		if (!Actor) { return false; }
		bool Installed = false;
		FPhysicsCommand::ExecuteWrite(Actor,[&](const FPhysicsActorHandle& LockedActor)
		{
			if (FPhysicsInterface::IsValid(LockedActor))
			{
				FPhysicsInterface::SetMaxLinearVelocity_AssumesLocked(LockedActor,MaxSpeedCmPerSecond);
				Installed = true;
			}
		});
		return Installed;
	}
}
