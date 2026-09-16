#pragma once

#include "CoreMinimal.h"

class AInteriorPortalSystem;
class UWorld;

/**
 * Stable production-facing control surface for the accepted FullFidelity portal renderer.
 *
 * Gameplay code should call this API directly. Console commands are retained only as
 * operator/diagnostic aliases and must not be required by the normal player lifecycle.
 */
namespace InteriorPortalFullFidelityRenderer
{
	/** True when this portal-system configuration delegates remote rendering to FullFidelity. */
	SLAYTHESPIREDEMO_API bool ShouldOwnRendering(const AInteriorPortalSystem* PortalSystem);

	/** Apply the production composition policy and start the FullFidelity backend for World. */
	SLAYTHESPIREDEMO_API bool Start(UWorld* World);

	/** Stop the FullFidelity backend and restore the composition policy that preceded Start(). */
	SLAYTHESPIREDEMO_API void Stop(UWorld* World);

	/** Dump the live renderer report. Intended for diagnostics/acceptance only. */
	SLAYTHESPIREDEMO_API void Dump(UWorld* World);
}
