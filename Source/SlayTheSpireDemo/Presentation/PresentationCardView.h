#pragma once

#include "../UI/BattleHUDTypes.h"

struct FPresentationCardSnapshot;

// Boundary adapter for committed historical card snapshots only.
// Do not use this projection for FCardReadView or FinalSnapshot formal-Hand
// freezing; those paths own current Gameplay legality and remain independent.
namespace PresentationCardView
{
	SLAYTHESPIREDEMO_API FBattleHUDCardView MakePresentationOnlyCardView(
		const FPresentationCardSnapshot& Snapshot);
}
