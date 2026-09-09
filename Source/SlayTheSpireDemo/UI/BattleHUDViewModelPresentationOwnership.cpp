#include "BattleHUDViewModel.h"

namespace
{
	bool IsSameRecordedWatermark(
		const FSelectionPresentationCompletionWatermark& Watermark,
		int64 BattleId,
		int64 SelectionGeneration,
		int64 BoundaryRevision,
		int64 ResolutionId)
	{
		return Watermark.Mode == ESelectionPresentationCompletionMode::RecordedResolution
			&& Watermark.BattleId == BattleId
			&& Watermark.SelectionGeneration == SelectionGeneration
			&& Watermark.BoundaryRevision == BoundaryRevision
			&& Watermark.ResolutionId == ResolutionId
			&& Watermark.StateRevision == 0;
	}

	bool IsSameDirectWatermark(
		const FSelectionPresentationCompletionWatermark& Watermark,
		int64 BattleId,
		int64 SelectionGeneration,
		int64 BoundaryRevision,
		int64 StateRevision)
	{
		return Watermark.Mode == ESelectionPresentationCompletionMode::DirectStateRevision
			&& Watermark.BattleId == BattleId
			&& Watermark.SelectionGeneration == SelectionGeneration
			&& Watermark.BoundaryRevision == BoundaryRevision
			&& Watermark.ResolutionId == 0
			&& Watermark.StateRevision == StateRevision;
	}

	bool IsValidForwardOwnershipTransfer(
		ECardPresentationOwner ExpectedOwner,
		ECardPresentationOwner NewOwner)
	{
		switch (ExpectedOwner)
		{
		case ECardPresentationOwner::SelectionArea:
			return NewOwner == ECardPresentationOwner::Transition;
		case ECardPresentationOwner::Transition:
			return NewOwner == ECardPresentationOwner::ConsumedPendingReducer;
		case ECardPresentationOwner::Hand:
		case ECardPresentationOwner::ConsumedPendingReducer:
		default:
			return false;
		}
	}
}

void UBattleHUDViewModel::SetPresentationDisplayOwned(bool bOwned)
{
	bPresentationDisplayOwned = bOwned;
}

int64 UBattleHUDViewModel::BeginCardPresentationSelectionLifecycle(
	int64 SelectionBoundaryRevision)
{
	if (BattleId <= 0
		|| SelectionBoundaryRevision <= 0
		|| SelectionBoundaryRevision != StateRevision
		|| ActiveCardPresentationSelectionGeneration != 0)
	{
		return 0;
	}

	if (NextCardPresentationSelectionGeneration <= 0)
	{
		NextCardPresentationSelectionGeneration = 1;
	}
	const int64 Generation = NextCardPresentationSelectionGeneration++;
	ActiveCardPresentationSelectionGeneration = Generation;
	ActiveCardPresentationSelectionBattleId = BattleId;
	ActiveCardPresentationSelectionBoundaryRevision = SelectionBoundaryRevision;
	return Generation;
}

bool UBattleHUDViewModel::CancelCardPresentationSelectionLifecycle(
	int64 SelectionGeneration)
{
	if (SelectionGeneration <= 0
		|| SelectionGeneration != ActiveCardPresentationSelectionGeneration
		|| BattleId != ActiveCardPresentationSelectionBattleId
		|| StateRevision != ActiveCardPresentationSelectionBoundaryRevision)
	{
		return false;
	}

	TArray<int32> ChangedRuntimeIds;
	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId == BattleId
			&& Entry.SelectionGeneration == SelectionGeneration
			&& Entry.SelectionBoundaryRevision == ActiveCardPresentationSelectionBoundaryRevision
			&& Entry.Phase == ESelectionPresentationVisualPhase::Pending
			&& Entry.Owner == ECardPresentationOwner::SelectionArea)
		{
			ChangedRuntimeIds.Add(Pair.Key);
		}
	}
	for (const int32 RuntimeId : ChangedRuntimeIds)
	{
		CardPresentationOwnershipEntries.Remove(RuntimeId);
	}

	ActiveCardPresentationSelectionGeneration = 0;
	ActiveCardPresentationSelectionBattleId = 0;
	ActiveCardPresentationSelectionBoundaryRevision = 0;
	PublishCardPresentationOwnershipChanged(ChangedRuntimeIds);
	return true;
}

bool UBattleHUDViewModel::SetPendingCardPresentationSelection(
	int64 SelectionGeneration,
	int32 RuntimeId,
	bool bSelected)
{
	if (SelectionGeneration <= 0
		|| RuntimeId == INDEX_NONE
		|| SelectionGeneration != ActiveCardPresentationSelectionGeneration
		|| BattleId != ActiveCardPresentationSelectionBattleId
		|| StateRevision != ActiveCardPresentationSelectionBoundaryRevision
		|| FindDisplayedCardByRuntimeId(RuntimeId) == nullptr)
	{
		return false;
	}

	FCardPresentationOwnershipEntry* Existing =
		CardPresentationOwnershipEntries.Find(RuntimeId);
	if (!bSelected)
	{
		if (Existing == nullptr
			|| Existing->BattleId != BattleId
			|| Existing->SelectionGeneration != SelectionGeneration
			|| Existing->SelectionBoundaryRevision != ActiveCardPresentationSelectionBoundaryRevision
			|| Existing->Phase != ESelectionPresentationVisualPhase::Pending
			|| Existing->Owner != ECardPresentationOwner::SelectionArea)
		{
			return false;
		}
		CardPresentationOwnershipEntries.Remove(RuntimeId);
		PublishCardPresentationOwnershipChanged({ RuntimeId });
		return true;
	}

	if (Existing != nullptr)
	{
		return Existing->BattleId == BattleId
			&& Existing->SelectionGeneration == SelectionGeneration
			&& Existing->SelectionBoundaryRevision ==
				ActiveCardPresentationSelectionBoundaryRevision
			&& Existing->Phase == ESelectionPresentationVisualPhase::Pending
			&& Existing->Owner == ECardPresentationOwner::SelectionArea;
	}

	FCardPresentationOwnershipEntry Entry;
	Entry.BattleId = BattleId;
	Entry.SelectionGeneration = SelectionGeneration;
	Entry.SelectionBoundaryRevision = ActiveCardPresentationSelectionBoundaryRevision;
	Entry.RuntimeId = RuntimeId;
	Entry.Owner = ECardPresentationOwner::SelectionArea;
	Entry.Phase = ESelectionPresentationVisualPhase::Pending;
	Entry.CompletionWatermark.Mode = ESelectionPresentationCompletionMode::Unresolved;
	Entry.CompletionWatermark.BattleId = BattleId;
	Entry.CompletionWatermark.SelectionGeneration = SelectionGeneration;
	Entry.CompletionWatermark.BoundaryRevision =
		ActiveCardPresentationSelectionBoundaryRevision;
	CardPresentationOwnershipEntries.Add(RuntimeId, Entry);
	PublishCardPresentationOwnershipChanged({ RuntimeId });
	return true;
}

bool UBattleHUDViewModel::ConfirmCardPresentationSelection(
	int64 SelectionGeneration,
	const TArray<int32>& RuntimeIds)
{
	if (SelectionGeneration <= 0
		|| RuntimeIds.IsEmpty()
		|| SelectionGeneration != ActiveCardPresentationSelectionGeneration
		|| BattleId != ActiveCardPresentationSelectionBattleId
		|| StateRevision != ActiveCardPresentationSelectionBoundaryRevision)
	{
		return false;
	}

	TSet<int32> ConfirmedIds;
	ConfirmedIds.Reserve(RuntimeIds.Num());
	for (const int32 RuntimeId : RuntimeIds)
	{
		if (RuntimeId == INDEX_NONE || ConfirmedIds.Contains(RuntimeId))
		{
			return false;
		}
		const FCardPresentationOwnershipEntry* Entry =
			CardPresentationOwnershipEntries.Find(RuntimeId);
		if (Entry == nullptr
			|| Entry->BattleId != BattleId
			|| Entry->SelectionGeneration != SelectionGeneration
			|| Entry->SelectionBoundaryRevision !=
				ActiveCardPresentationSelectionBoundaryRevision
			|| Entry->Phase != ESelectionPresentationVisualPhase::Pending
			|| Entry->Owner != ECardPresentationOwner::SelectionArea)
		{
			return false;
		}
		ConfirmedIds.Add(RuntimeId);
	}

	TArray<int32> ChangedRuntimeIds;
	TArray<int32> PendingIdsToRestore;
	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId == BattleId
			&& Entry.SelectionGeneration == SelectionGeneration
			&& Entry.SelectionBoundaryRevision == ActiveCardPresentationSelectionBoundaryRevision
			&& Entry.Phase == ESelectionPresentationVisualPhase::Pending
			&& Entry.Owner == ECardPresentationOwner::SelectionArea
			&& !ConfirmedIds.Contains(Pair.Key))
		{
			PendingIdsToRestore.Add(Pair.Key);
		}
	}
	for (const int32 RuntimeId : PendingIdsToRestore)
	{
		CardPresentationOwnershipEntries.Remove(RuntimeId);
		ChangedRuntimeIds.Add(RuntimeId);
	}

	for (const int32 RuntimeId : RuntimeIds)
	{
		FCardPresentationOwnershipEntry& Entry =
			CardPresentationOwnershipEntries.FindChecked(RuntimeId);
		Entry.Phase = ESelectionPresentationVisualPhase::Confirmed;
		Entry.CompletionWatermark = FSelectionPresentationCompletionWatermark{};
		Entry.CompletionWatermark.Mode = ESelectionPresentationCompletionMode::Unresolved;
		Entry.CompletionWatermark.BattleId = BattleId;
		Entry.CompletionWatermark.SelectionGeneration = SelectionGeneration;
		Entry.CompletionWatermark.BoundaryRevision = Entry.SelectionBoundaryRevision;
		ChangedRuntimeIds.Add(RuntimeId);
	}

	ActiveCardPresentationSelectionGeneration = 0;
	ActiveCardPresentationSelectionBattleId = 0;
	ActiveCardPresentationSelectionBoundaryRevision = 0;
	PublishCardPresentationOwnershipChanged(ChangedRuntimeIds);
	return true;
}

bool UBattleHUDViewModel::TryTransferCardPresentationOwnership(
	int64 SelectionGeneration,
	int32 RuntimeId,
	ECardPresentationOwner ExpectedOwner,
	ECardPresentationOwner NewOwner)
{
	FCardPresentationOwnershipEntry* Entry =
		CardPresentationOwnershipEntries.Find(RuntimeId);
	if (SelectionGeneration <= 0
		|| Entry == nullptr
		|| Entry->BattleId != BattleId
		|| Entry->SelectionGeneration != SelectionGeneration
		|| Entry->Owner != ExpectedOwner
		|| Entry->Phase != ESelectionPresentationVisualPhase::Confirmed
		|| !IsValidForwardOwnershipTransfer(ExpectedOwner, NewOwner))
	{
		return false;
	}

	Entry->Owner = NewOwner;
	PublishCardPresentationOwnershipChanged({ RuntimeId });
	return true;
}

bool UBattleHUDViewModel::ArmRecordedCardPresentationCompletion(
	int64 SelectionGeneration,
	int64 ResolutionId)
{
	if (SelectionGeneration <= 0 || ResolutionId <= 0 || BattleId <= 0)
	{
		return false;
	}

	TArray<int32> MatchingRuntimeIds;
	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.SelectionGeneration != SelectionGeneration
			|| Entry.Phase != ESelectionPresentationVisualPhase::Confirmed)
		{
			continue;
		}
		if (Entry.CompletionWatermark.IsResolved()
			&& !IsSameRecordedWatermark(
				Entry.CompletionWatermark,
				Entry.BattleId,
				Entry.SelectionGeneration,
				Entry.SelectionBoundaryRevision,
				ResolutionId))
		{
			return false;
		}
		MatchingRuntimeIds.Add(Pair.Key);
	}
	if (MatchingRuntimeIds.IsEmpty())
	{
		return false;
	}

	TArray<int32> ChangedRuntimeIds;
	for (const int32 RuntimeId : MatchingRuntimeIds)
	{
		FCardPresentationOwnershipEntry& Entry =
			CardPresentationOwnershipEntries.FindChecked(RuntimeId);
		if (Entry.CompletionWatermark.IsResolved())
		{
			continue;
		}
		Entry.CompletionWatermark.Mode =
			ESelectionPresentationCompletionMode::RecordedResolution;
		Entry.CompletionWatermark.BattleId = Entry.BattleId;
		Entry.CompletionWatermark.SelectionGeneration = Entry.SelectionGeneration;
		Entry.CompletionWatermark.BoundaryRevision = Entry.SelectionBoundaryRevision;
		Entry.CompletionWatermark.ResolutionId = ResolutionId;
		Entry.CompletionWatermark.StateRevision = 0;
		ChangedRuntimeIds.Add(RuntimeId);
	}
	PublishCardPresentationOwnershipChanged(ChangedRuntimeIds);
	ReconcileCardPresentationOwnership();
	return true;
}

bool UBattleHUDViewModel::ArmDirectCardPresentationCompletion(
	int64 SelectionGeneration,
	int64 PostConfirmStateRevision)
{
	if (SelectionGeneration <= 0
		|| PostConfirmStateRevision <= 0
		|| BattleId <= 0)
	{
		return false;
	}

	TArray<int32> MatchingRuntimeIds;
	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.SelectionGeneration != SelectionGeneration
			|| Entry.Phase != ESelectionPresentationVisualPhase::Confirmed)
		{
			continue;
		}
		if (PostConfirmStateRevision <= Entry.SelectionBoundaryRevision)
		{
			return false;
		}
		if (Entry.CompletionWatermark.IsResolved()
			&& !IsSameDirectWatermark(
				Entry.CompletionWatermark,
				Entry.BattleId,
				Entry.SelectionGeneration,
				Entry.SelectionBoundaryRevision,
				PostConfirmStateRevision))
		{
			return false;
		}
		MatchingRuntimeIds.Add(Pair.Key);
	}
	if (MatchingRuntimeIds.IsEmpty())
	{
		return false;
	}

	TArray<int32> ChangedRuntimeIds;
	for (const int32 RuntimeId : MatchingRuntimeIds)
	{
		FCardPresentationOwnershipEntry& Entry =
			CardPresentationOwnershipEntries.FindChecked(RuntimeId);
		if (Entry.CompletionWatermark.IsResolved())
		{
			continue;
		}
		Entry.CompletionWatermark.Mode =
			ESelectionPresentationCompletionMode::DirectStateRevision;
		Entry.CompletionWatermark.BattleId = Entry.BattleId;
		Entry.CompletionWatermark.SelectionGeneration = Entry.SelectionGeneration;
		Entry.CompletionWatermark.BoundaryRevision = Entry.SelectionBoundaryRevision;
		Entry.CompletionWatermark.ResolutionId = 0;
		Entry.CompletionWatermark.StateRevision = PostConfirmStateRevision;
		ChangedRuntimeIds.Add(RuntimeId);
	}
	PublishCardPresentationOwnershipChanged(ChangedRuntimeIds);
	ReconcileCardPresentationOwnership();
	return true;
}

void UBattleHUDViewModel::MarkPresentationResolutionCompleted(
	int64 InBattleId,
	int64 ResolutionId)
{
	if (InBattleId <= 0 || ResolutionId <= 0 || InBattleId != BattleId)
	{
		return;
	}
	if (CompletedPresentationResolutionBattleId != InBattleId)
	{
		CompletedPresentationResolutionBattleId = InBattleId;
		CompletedPresentationResolutionIds.Reset();
	}
	CompletedPresentationResolutionIds.Add(ResolutionId);
	ReconcileCardPresentationOwnership();
}

void UBattleHUDViewModel::ReconcileCardPresentationOwnership()
{
	PublishCardPresentationOwnershipChanged(
		ReconcileCardPresentationOwnershipInternal());
}

TArray<int32> UBattleHUDViewModel::ReconcileCardPresentationOwnershipInternal()
{
	TArray<int32> EntriesToClear;

	const bool bActivePendingLifecycleStale =
		ActiveCardPresentationSelectionGeneration != 0
		&& (ActiveCardPresentationSelectionBattleId != BattleId
			|| ActiveCardPresentationSelectionBoundaryRevision != StateRevision);
	if (bActivePendingLifecycleStale)
	{
		ActiveCardPresentationSelectionGeneration = 0;
		ActiveCardPresentationSelectionBattleId = 0;
		ActiveCardPresentationSelectionBoundaryRevision = 0;
	}

	if (CardPresentationOwnershipEntries.IsEmpty())
	{
		return EntriesToClear;
	}

	TSet<int32> DisplayedHandRuntimeIds;
	DisplayedHandRuntimeIds.Reserve(HandCards.Num());
	for (const FBattleHUDCardView& CardView : HandCards)
	{
		if (CardView.RuntimeId != INDEX_NONE)
		{
			DisplayedHandRuntimeIds.Add(CardView.RuntimeId);
		}
	}

	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.RuntimeId == INDEX_NONE
			|| !DisplayedHandRuntimeIds.Contains(Entry.RuntimeId)
			|| (Entry.Phase == ESelectionPresentationVisualPhase::Pending
				&& Entry.SelectionBoundaryRevision != StateRevision))
		{
			EntriesToClear.Add(Pair.Key);
			continue;
		}

		// Formal lifecycle completion is the final fail-safe. Once the exact
		// recorded/direct watermark is reached, no destination outcome from this
		// lifecycle may still arrive. Any still-present Hand card therefore returns
		// to Hand ownership even if a failed timeout/collapse left the transient
		// owner at Transition or ConsumedPendingReducer.
		if (Entry.Owner != ECardPresentationOwner::Hand
			&& Entry.Phase == ESelectionPresentationVisualPhase::Confirmed
			&& Entry.CompletionWatermark.IsResolved()
			&& IsCardPresentationCompletionWatermarkReached(Entry))
		{
			EntriesToClear.Add(Pair.Key);
		}
	}

	for (const int32 RuntimeId : EntriesToClear)
	{
		CardPresentationOwnershipEntries.Remove(RuntimeId);
	}
	return EntriesToClear;
}

ECardPresentationOwner UBattleHUDViewModel::GetCardPresentationOwner(int32 RuntimeId) const
{
	if (const FCardPresentationOwnershipEntry* Entry =
		CardPresentationOwnershipEntries.Find(RuntimeId))
	{
		return Entry->Owner;
	}
	return ECardPresentationOwner::Hand;
}

bool UBattleHUDViewModel::TryGetCardPresentationOwnershipEntry(
	int32 RuntimeId,
	FCardPresentationOwnershipEntry& OutEntry) const
{
	OutEntry = FCardPresentationOwnershipEntry{};
	if (const FCardPresentationOwnershipEntry* Entry =
		CardPresentationOwnershipEntries.Find(RuntimeId))
	{
		OutEntry = *Entry;
		return true;
	}
	return false;
}

bool UBattleHUDViewModel::IsCardPresentationCompletionWatermarkReached(
	const FCardPresentationOwnershipEntry& Entry) const
{
	const FSelectionPresentationCompletionWatermark& Watermark =
		Entry.CompletionWatermark;
	if (!Watermark.IsResolved()
		|| Watermark.BattleId != BattleId
		|| Watermark.BattleId != Entry.BattleId
		|| Watermark.SelectionGeneration != Entry.SelectionGeneration
		|| Watermark.BoundaryRevision != Entry.SelectionBoundaryRevision)
	{
		return false;
	}

	switch (Watermark.Mode)
	{
	case ESelectionPresentationCompletionMode::RecordedResolution:
		return Watermark.ResolutionId > 0
			&& CompletedPresentationResolutionBattleId == Watermark.BattleId
			&& CompletedPresentationResolutionIds.Contains(Watermark.ResolutionId);

	case ESelectionPresentationCompletionMode::DirectStateRevision:
		return Watermark.StateRevision > Entry.SelectionBoundaryRevision
			&& BattleId == Watermark.BattleId
			&& StateRevision >= Watermark.StateRevision;

	case ESelectionPresentationCompletionMode::Unresolved:
	default:
		return false;
	}
}

void UBattleHUDViewModel::PublishCardPresentationOwnershipChanged(
	const TArray<int32>& ChangedRuntimeIds)
{
	if (ChangedRuntimeIds.IsEmpty())
	{
		return;
	}

	TArray<int32> UniqueRuntimeIds;
	UniqueRuntimeIds.Reserve(ChangedRuntimeIds.Num());
	for (const int32 RuntimeId : ChangedRuntimeIds)
	{
		if (RuntimeId != INDEX_NONE && !UniqueRuntimeIds.Contains(RuntimeId))
		{
			UniqueRuntimeIds.Add(RuntimeId);
		}
	}
	if (!UniqueRuntimeIds.IsEmpty())
	{
		OnCardPresentationOwnershipChanged.Broadcast(UniqueRuntimeIds);
	}
}

void UBattleHUDViewModel::ResetCardPresentationOwnershipState(bool bNotify)
{
	TArray<int32> ChangedRuntimeIds;
	if (bNotify)
	{
		CardPresentationOwnershipEntries.GetKeys(ChangedRuntimeIds);
	}
	CardPresentationOwnershipEntries.Reset();
	ActiveCardPresentationSelectionGeneration = 0;
	ActiveCardPresentationSelectionBattleId = 0;
	ActiveCardPresentationSelectionBoundaryRevision = 0;
	CompletedPresentationResolutionBattleId = 0;
	CompletedPresentationResolutionIds.Reset();
	PublishCardPresentationOwnershipChanged(ChangedRuntimeIds);
}
