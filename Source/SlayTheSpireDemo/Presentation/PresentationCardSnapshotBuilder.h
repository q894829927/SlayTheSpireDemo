#pragma once

#include "CoreMinimal.h"
#include "PresentationTypes.h"

class ACombatant;
class UCardInstance;

namespace PresentationCardSnapshot
{
	SLAYTHESPIREDEMO_API bool TryBuild(
		const UCardInstance* Card,
		ACombatant* Source,
		FPresentationCardSnapshot& OutSnapshot
	);
}
