#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleSelectionRequest.h"

bool UBattleHUDViewModel::TryGetPendingCardSelectionReadView(
	FPendingCardSelectionReadView& OutView
) const
{
	return BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
		BattleManager.Get(),
		OutView
	);
}

bool UBattleHUDViewModel::HasPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	return TryGetPendingCardSelectionReadView(View);
}

bool UBattleHUDViewModel::IsPendingCardSelectionCandidate(int32 RuntimeId) const
{
	FPendingCardSelectionReadView View;
	return RuntimeId != INDEX_NONE
		&& TryGetPendingCardSelectionReadView(View)
		&& View.CandidateRuntimeIds.Contains(RuntimeId);
}

bool UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeIds(
	const TArray<int32>& RuntimeIds
)
{
	FPendingCardSelectionReadView View;
	ABattleManager* Battle = BattleManager.Get();
	if (!TryGetPendingCardSelectionReadView(View)
		|| RuntimeIds.Num() != View.RequiredCount)
	{
		return false;
	}

	const bool bSubmitted = BattleSelectionRequest::SubmitPendingCardSelection(Battle, RuntimeIds);
	if (bSubmitted)
	{
		PendingCardSelectionRuntimeIds.Reset();
	}
	return bSubmitted;
}

bool UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeId(int32 RuntimeId)
{
	FPendingCardSelectionReadView View;
	if (RuntimeId == INDEX_NONE
		|| !TryGetPendingCardSelectionReadView(View)
		|| View.RequiredCount <= 0
		|| !View.CandidateRuntimeIds.Contains(RuntimeId))
	{
		return false;
	}

	// Remove stale local ids if the pending request changed underneath the UI.
	PendingCardSelectionRuntimeIds.RemoveAll(
		[&View](int32 SelectedRuntimeId)
		{
			return !View.CandidateRuntimeIds.Contains(SelectedRuntimeId);
		}
	);

	const int32 ExistingIndex = PendingCardSelectionRuntimeIds.Find(RuntimeId);
	if (ExistingIndex != INDEX_NONE)
	{
		PendingCardSelectionRuntimeIds.RemoveAt(ExistingIndex);
		return true;
	}

	if (PendingCardSelectionRuntimeIds.Num() >= View.RequiredCount)
	{
		PendingCardSelectionRuntimeIds.Reset();
		return false;
	}

	PendingCardSelectionRuntimeIds.Add(RuntimeId);
	if (PendingCardSelectionRuntimeIds.Num() < View.RequiredCount)
	{
		return true;
	}

	// Reaching exactly N auto-submits. Gameplay facade canonicalizes the final
	// result into authoritative candidate order, so click order is UI-only.
	const TArray<int32> CompletedRuntimeIds = PendingCardSelectionRuntimeIds;
	const bool bSubmitted = SubmitPendingCardSelectionByRuntimeIds(CompletedRuntimeIds);
	if (!bSubmitted)
	{
		PendingCardSelectionRuntimeIds.Reset();
	}
	return bSubmitted;
}

bool UBattleHUDViewModel::IsPendingCardSelectionRuntimeIdSelected(int32 RuntimeId) const
{
	return RuntimeId != INDEX_NONE
		&& PendingCardSelectionRuntimeIds.Contains(RuntimeId);
}

int32 UBattleHUDViewModel::GetPendingCardSelectionSelectedCount() const
{
	return PendingCardSelectionRuntimeIds.Num();
}

void UBattleHUDViewModel::ClearPendingCardSelectionInputState()
{
	PendingCardSelectionRuntimeIds.Reset();
}

bool UBattleHUDViewModel::CanCancelPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	return TryGetPendingCardSelectionReadView(View) && View.bCanCancel;
}

bool UBattleHUDViewModel::SubmitPendingCardSelectionCancel()
{
	ABattleManager* Battle = BattleManager.Get();
	const bool bCancelled = CanCancelPendingCardSelection()
		&& BattleSelectionRequest::SubmitPendingSelectionCancel(Battle);
	if (bCancelled)
	{
		PendingCardSelectionRuntimeIds.Reset();
	}
	return bCancelled;
}
