#pragma once

#include "CoreMinimal.h"

struct FDamagePresentationPayload;
struct FPresentationStateSnapshot;

// Single historical Damage rule shared by current Blocking playback and the
// future detached transaction. It mutates only the supplied frozen snapshot and
// has no Gameplay/UI/cosmetic side effects.
namespace PresentationDamageReducer
{
	SLAYTHESPIREDEMO_API bool TryApplyDamageRecord(
		FPresentationStateSnapshot& Snapshot,
		const FDamagePresentationPayload& Damage);
}
