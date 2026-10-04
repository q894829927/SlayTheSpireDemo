#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Native backend seam for the accepted endpoint x recursion FullFidelity renderer.
 *
 * This deliberately keeps the historical MultiVisibleTSR implementation file and
 * diagnostic console aliases intact while removing console-command dispatch from
 * normal FullFidelity control flow.
 */
namespace InteriorPortalFullFidelityBackend
{
	/** Start the accepted endpoint x recursion renderer for the active PIE/Game world. */
	SLAYTHESPIREDEMO_API bool Start(UWorld* World);

	/** True while the accepted endpoint x recursion backend owns rendering. */
	SLAYTHESPIREDEMO_API bool IsRunning();

	/** Stop the accepted renderer and release its endpoint x recursion resources. */
	SLAYTHESPIREDEMO_API void Stop();

	/** Dump live endpoint/recursion telemetry. */
	SLAYTHESPIREDEMO_API void Dump();
}
