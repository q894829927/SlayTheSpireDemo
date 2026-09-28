#pragma once

#include "InteriorPortalPhysicsBoundary.h"

class UWorld;
class UPrimitiveComponent;

namespace InteriorPortalPhysics
{
	/** Game-thread inputs for one immutable body/pair binding. This does not grant a solver permission. */
	struct FPhysicsBindingRequest
	{
		UWorld* World = nullptr;
		UPrimitiveComponent* Body = nullptr;
		UPrimitiveComponent* Supports[2] = {nullptr,nullptr};
		const FTravellerRegistry* Registry = nullptr;
		FVector2D EndpointApertures[2] = {FVector2D(65,115),FVector2D(65,115)};
		FBoundaryCommand Command;
	};
	struct FPreparedPhysicsBinding
	{
		FBoundaryCommand Command;
		FGeometry SupportGeometry[2];
		FTransform SupportPose[2];
	};
	enum class EPhysicsBindingResult : uint8
	{
		Ready, InvalidRequest, StaleTraveller, UnsupportedSupport, IncompatibleAperture
	};
	/** Snapshot registered geometry, support collision and native COM before PT binding.
	 * The caller must install a native speed cap and bind the current proxies;
	 * FChaosStaticClearance independently rechecks all native state each substep.
	 */
	SLAYTHESPIREDEMO_API EPhysicsBindingResult PreparePhysicsBinding_GameThread(
		const FPhysicsBindingRequest& Request, FPreparedPhysicsBinding& Out);
}
