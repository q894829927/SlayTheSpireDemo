#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleSelectionRequest.h"

bool UBattleHUDViewModel::TryGetPendingCardSelectionReadView(
	FPendingCardSelectionReadView& OutView
) const
{
	if (bSelectionCorrelationFailed) return false;
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

	if (!BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Battle, OutView)
		|| !OutView.RequestIdentity.IsValid()
		|| OutView.RequestIdentity.BattleId != BattleId
		|| OutView.RequestIdentity.SelectionBoundaryRevision != StateRevision)
	{
		OutView = FPendingCardSelectionReadView{};
		return false;
	}
	return true;
}

bool UBattleHUDViewModel::HasPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	return TryGetPendingCardSelectionReadView(View);
}

bool UBattleHUDViewModel::HasAuthoritativePendingCardSelection() const
{
	ABattleManager* Battle = BattleManager.Get();
	FPendingCardSelectionReadView View;
	return IsValid(Battle)
		&& BattleSelectionRequest::TryBuildPendingCardSelectionReadView(Battle, View)
		&& View.RequestIdentity.IsValid();
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

	const bool bSubmitted = BattleSelectionRequest::SubmitPendingCardSelection(
		Battle,
		View.RequestIdentity,
		RuntimeIds);
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
		PendingCardSelectionRequestIdentity != View.RequestIdentity
		|| PendingCardSelectionSource != View.SelectionSource
		|| PendingCardSelectionRequiredCount != View.RequiredCount
		|| PendingCardSelectionCandidateRuntimeIds != View.CandidateRuntimeIds;
	if (bRequestChanged)
	{
		PendingCardSelectionRuntimeIds.Reset();
		PendingCardSelectionRequestIdentity = View.RequestIdentity;
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

	// Exact-N selection and explicit confirmation are separate player intents.
	// Once N cards are selected, an unselected candidate cannot silently replace
	// or clear that transient set. The player must deselect one card first.
	if (PendingCardSelectionRuntimeIds.Num() >= View.RequiredCount)
	{
		return false;
	}

	PendingCardSelectionRuntimeIds.Add(RuntimeId);
	return true;
}

bool UBattleHUDViewModel::CanConfirmPendingCardSelection() const
{
	FPendingCardSelectionReadView View;
	if (!TryGetPendingCardSelectionReadView(View)
		|| View.RequiredCount <= 0
		|| PendingCardSelectionRequestIdentity != View.RequestIdentity
		|| PendingCardSelectionRuntimeIds.Num() != View.RequiredCount)
	{
		return false;
	}

	for (const int32 RuntimeId : PendingCardSelectionRuntimeIds)
	{
		if (RuntimeId == INDEX_NONE || !View.CandidateRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}
	}
	return true;
}

bool UBattleHUDViewModel::ConfirmPendingCardSelection()
{
	if (ActiveCardPresentationSelectionGeneration > 0)
		return ConfirmPendingCardSelectionWithPresentation(ActiveCardPresentationSelectionGeneration);
	if (!CanConfirmPendingCardSelection())
	{
		return false;
	}

	// Gameplay facade canonicalizes this transient UI set into authoritative
	// candidate order. Click order never controls Gameplay ordering.
	const TArray<int32> ConfirmedRuntimeIds = PendingCardSelectionRuntimeIds;
	return SubmitPendingCardSelectionByRuntimeIds(ConfirmedRuntimeIds);
}

bool UBattleHUDViewModel::IsPendingCardSelectionRuntimeIdSelected(int32 RuntimeId) const
{
	FPendingCardSelectionReadView View;
	return RuntimeId != INDEX_NONE
		&& TryGetPendingCardSelectionReadView(View)
		&& PendingCardSelectionRequestIdentity == View.RequestIdentity
		&& PendingCardSelectionRuntimeIds.Contains(RuntimeId);
}

int32 UBattleHUDViewModel::GetPendingCardSelectionSelectedCount() const
{
	FPendingCardSelectionReadView View;
	return TryGetPendingCardSelectionReadView(View)
		&& PendingCardSelectionRequestIdentity == View.RequestIdentity
		? PendingCardSelectionRuntimeIds.Num()
		: 0;
}

void UBattleHUDViewModel::ClearPendingCardSelectionInputState()
{
	PendingCardSelectionRuntimeIds.Reset();
	PendingCardSelectionRequestIdentity = FPendingSelectionRequestIdentity{};
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
	FPendingCardSelectionReadView View;
	ABattleManager* Battle = BattleManager.Get();
	const bool bCancelled = TryGetPendingCardSelectionReadView(View)
		&& View.bCanCancel
		&& BattleSelectionRequest::SubmitPendingSelectionCancel(Battle, View.RequestIdentity);
	if (bCancelled)
	{
		ClearPendingCardSelectionInputState();
	}
	return bCancelled;
}
