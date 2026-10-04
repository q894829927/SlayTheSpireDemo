#pragma once

#include "CoreMinimal.h"

struct FBodyInstance;

namespace InteriorPortalPhysics
{
	/** Installs the integration bound on the current Chaos body at a GT binding boundary.
	 * Rebind and reapply after actual body recreation; the PT clearance independently verifies it
	 * at every stage and refuses to use a missing or changed bound.
	 */
	class SLAYTHESPIREDEMO_API FChaosTravellerSpeedCap
	{
	public:
		static bool Install_GameThread(FBodyInstance& Body, float MaxSpeedCmPerSecond);
	};
}
