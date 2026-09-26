#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InteriorPortalTravellerRegistry.h"

class AInteriorPortalSystem;
class AInteriorPortal;

/**
 * Bounded world queries that continue through the authored portal pair.
 * Portal surfaces are decorative and have no collision; support-wall hits in
 * the legal aperture are therefore replaced by an analytic portal event.
 */
namespace InteriorPortalQuery
{
	/** Read-only PHY-1 eligibility shared by held/free body consumers. */
	SLAYTHESPIREDEMO_API InteriorPortalPhysics::FFitResult EvaluateBodyPassage(
		const AInteriorPortalSystem* System, const InteriorPortalPhysics::FTravellerSnapshot& Snapshot,
		const FTransform& From, const FTransform& To, const AInteriorPortal* Portal,
		uint64 PairGeneration, double MarginCm = .5);
	SLAYTHESPIREDEMO_API bool LineTrace(
		const AInteriorPortalSystem* System,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params,
		FHitResult& OutHit,
		int32 MaxPortalHops = 3,
		float PortalBias = 1.0f);

	SLAYTHESPIREDEMO_API bool SphereSweep(
		const AInteriorPortalSystem* System,
		const FVector& Start,
		const FVector& End,
		float Radius,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params,
		FHitResult& OutHit,
		int32 MaxPortalHops = 3,
		float PortalBias = 1.0f);
}
