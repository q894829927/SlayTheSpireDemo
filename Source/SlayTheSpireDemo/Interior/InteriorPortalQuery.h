#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AInteriorPortalSystem;

/**
 * Bounded world queries that continue through the authored portal pair.
 * Portal surfaces are decorative and have no collision; support-wall hits in
 * the legal aperture are therefore replaced by an analytic portal event.
 */
namespace InteriorPortalQuery
{
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
