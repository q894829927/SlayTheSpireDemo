#pragma once

#include "CoreMinimal.h"
#include "../Battle/BattleReadSnapshot.h"
#include "../UI/BattleHUDTypes.h"

namespace RelicPresentationSnapshot
{
	// Converts exact read-revision Relic facts into immutable player-facing HUD
	// DTOs. The output must not retain URelicInstance or other mutable Gameplay
	// runtime pointers.
	SLAYTHESPIREDEMO_API bool TryFreeze(
		const TArray<FRelicReadView>& Relics,
		TArray<FBattleHUDRelicView>& OutRelics);
}
