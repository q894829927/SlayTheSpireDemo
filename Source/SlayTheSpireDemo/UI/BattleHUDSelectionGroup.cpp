#include "BattleHUDSelectionWidget.h"

#include "../Presentation/BattlePresentationController.h"

bool UBattleHUDSelectionWidget::BeginPresentationRecordPlayback_Implementation(
	const FPresentationRecord& Record,
	const FPresentationPlaybackToken& Token)
{
	if (IsValid(PresentationController))
	{
		// A future member that already finished its parallel visual must not replay.
		// Returning false deliberately uses the Controller's existing immediate
		// fallback, which applies the exact record to the working snapshot now.
		if (PresentationController->ConsumeVisuallyPresentedGroupRecordG6(
			Record,
			Token))
		{
			return false;
		}

		if (Record.Group.IsValid()
			&& Record.Group.Kind == EPresentationGroupKind::SelectionDestination
			&& Record.Group.ExpectedMemberCount > 1
			&& PresentationController->TryActivatePresentationGroupG6(
				Record,
				Token))
		{
			return true;
		}
	}

	// Semantic rejection, visual-preflight rejection, or ordinary records retain
	// the sealed G5/G4 SingleRecord behavior unchanged.
	return Super::BeginPresentationRecordPlayback_Implementation(Record, Token);
}
