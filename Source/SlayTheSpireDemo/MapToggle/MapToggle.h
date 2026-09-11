#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace MapToggle
{
	/** Switches between the default battle map and the interior exploration map. */
	SLAYTHESPIREDEMO_API void Toggle(UWorld* World);

	/** Re-arms the toggle after the M key has been released. */
	SLAYTHESPIREDEMO_API void HandleReleased(UWorld* World);
}
