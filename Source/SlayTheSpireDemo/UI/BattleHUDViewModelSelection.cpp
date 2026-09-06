#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleSelectionRequest.h"

bool UBattleHUDViewModel::HasPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	return BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
		BattleManager.Get(),
		View
	);
}

bool UBattleHUDViewModel::IsPendingCardSelectionCandidate(int32 RuntimeId) const
{
	FPendingCardSelectionReadView View;
	return RuntimeId != INDEX_NONE
		&& BattleSelectionRequest::TryBuildPendingCardSelectionReadView(BattleManager.Get(), View)
		&& View.CandidateRuntimeIds.Contains(RuntimeId);
}

bool UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeId(int32 RuntimeId)
{
	FPendingCardSelectionReadView View;
	ABattleManager* Battle = BattleManager.Get();
	if (RuntimeId == INDEX_NONE
		|| !BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Battle, View)
		|| !View.CandidateRuntimeIds.Contains(RuntimeId))
	{
		return false;
	}

	// This is input into the already-active Gameplay resolution, not a new card
	// play command. Historical display remains frozen until normal committed
	// Presentation catches up after the resolution finishes.
	return BattleSelectionRequest::SubmitPendingCardSelection(Battle, RuntimeId);
}

bool UBattleHUDViewModel::CanCancelPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	return BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
		BattleManager.Get(),
		View
	) && View.bCanCancel;
}

bool UBattleHUDViewModel::SubmitPendingCardSelectionCancel()
{
	ABattleManager* Battle = BattleManager.Get();
	return CanCancelPendingCardSelection()
		&& BattleSelectionRequest::SubmitPendingSelectionCancel(Battle);
}
