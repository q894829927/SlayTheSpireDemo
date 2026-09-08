#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleSelectionRequest.h"

bool UBattleHUDViewModel::TryGetPendingCardSelectionReadView(
	FPendingCardSelectionReadView& OutView
) const
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle) || !Battle->IsPresentationAvailable())
	{
		return false;
	}

	// Gameplay may already be waiting on an authoritative selection while the
	// Native committed Presentation is still showing the effects that produced
	// the candidate Hand. Do not expose that pending request to player input until
	// the displayed frozen revision reaches the newest sealed boundary.
	// Direct/no-history display also waits for its frozen read edge; otherwise
	// a click could submit the new request against the preceding Hand snapshot.
	{
		FPresentationStateSnapshot LatestBaseline;
		if (!Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline)
			|| LatestBaseline.BattleId != BattleId
			|| LatestBaseline.StateRevision != StateRevision)
		{
			return false;
		}
	}

	return BattleSelectionRequest::TryBuildPendingCardSelectionReadView(
		Battle,
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
		ClearPendingCardSelectionInputState();
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

	const bool bRequestChanged =
		PendingCardSelectionSource != View.SelectionSource
		|| PendingCardSelectionRequiredCount != View.RequiredCount
		|| PendingCardSelectionCandidateRuntimeIds != View.CandidateRuntimeIds;
	if (bRequestChanged)
	{
		PendingCardSelectionRuntimeIds.Reset();
		PendingCardSelectionSource = View.SelectionSource;
		PendingCardSelectionRequiredCount = View.RequiredCount;
		PendingCardSelectionCandidateRuntimeIds = View.CandidateRuntimeIds;
	}

	const int32 ExistingIndex = PendingCardSelectionRuntimeIds.Find(RuntimeId);
	if (ExistingIndex != INDEX_NONE)
	{
		PendingCardSelectionRuntimeIds.RemoveAt(ExistingIndex);
		return true;
	}

	if (PendingCardSelectionRuntimeIds.Num() >= View.RequiredCount)
	{
		ClearPendingCardSelectionInputState();
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
		ClearPendingCardSelectionInputState();
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
	PendingCardSelectionSource = NAME_None;
	PendingCardSelectionRequiredCount = 0;
	PendingCardSelectionCandidateRuntimeIds.Reset();
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
		ClearPendingCardSelectionInputState();
	}
	return bCancelled;
}
