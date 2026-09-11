#pragma once

#include "CoreMinimal.h"

// One timing authority for the legacy Blocking Damage wait and the G8-C
// compatibility debt increment. G8-D may stop consuming this for readiness,
// but G8-C must keep both paths duration-compatible.
namespace PresentationDamageTiming
{
	SLAYTHESPIREDEMO_API float GetLegacyDamageBlockingDuration();
}
