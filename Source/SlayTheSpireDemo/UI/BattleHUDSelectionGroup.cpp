#include "BattleHUDSelectionWidget.h"

#include "../Presentation/BattlePresentationController.h"

bool UBattleHUDSelectionWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	// Shared transition dispatch also owns formal-Hand groups. Selection supplies
	// only its source adapter, without an independent playback dispatch table.
	return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
}
