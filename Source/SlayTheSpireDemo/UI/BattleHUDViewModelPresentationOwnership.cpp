#include "BattleHUDViewModel.h"

void UBattleHUDViewModel::SetPresentationDisplayOwned(bool bOwned)
{
	bPresentationDisplayOwned = bOwned;
}

int64 UBattleHUDViewModel::BeginCardPresentationSelectionLifecycle(
	int64 SelectionBoundaryRevision)
{
	if (BattleId <= 0
		|| SelectionBoundaryRevision <= 0
		|| SelectionBoundaryRevision != StateRevision)
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

bool UBattleHUDViewModel::SetPendingCardPresentationSelection(
	int64 SelectionGeneration,
	int32 RuntimeId,
	bool bSelected)
{
	if (SelectionGeneration <= 0
		|| RuntimeId == INDEX_NONE
		|| SelectionGeneration != ActiveCardPresentationSelectionGeneration
		|| BattleId != ActiveCardPresentationSelectionBattleId
		|| StateRevision != ActiveCardPresentationSelectionBoundaryRevision)
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
			&& Entry.Phase == ESelectionPresentationVisualPhase::Pending
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
	if (Entry == nullptr
		|| Entry->BattleId != BattleId
		|| Entry->SelectionGeneration != SelectionGeneration
		|| Entry->Owner != ExpectedOwner
		|| Entry->Phase == ESelectionPresentationVisualPhase::None)
	{
		return false;
	}

	if (NewOwner == ECardPresentationOwner::Hand)
	{
		CardPresentationOwnershipEntries.Remove(RuntimeId);
	}
	else
	{
		Entry->Owner = NewOwner;
	}
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

	TArray<int32> ChangedRuntimeIds;
	for (TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.SelectionGeneration != SelectionGeneration
			|| Entry.Phase != ESelectionPresentationVisualPhase::Confirmed)
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
		ChangedRuntimeIds.Add(Pair.Key);
	}
	if (ChangedRuntimeIds.IsEmpty())
	{
		return false;
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

	TArray<int32> ChangedRuntimeIds;
	for (TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.SelectionGeneration != SelectionGeneration
			|| Entry.Phase != ESelectionPresentationVisualPhase::Confirmed
			|| PostConfirmStateRevision < Entry.SelectionBoundaryRevision)
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
		ChangedRuntimeIds.Add(Pair.Key);
	}
	if (ChangedRuntimeIds.IsEmpty())
	{
		return false;
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
	if (CardPresentationOwnershipEntries.IsEmpty())
	{
		return;
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

	TArray<int32> EntriesToClear;
	for (const TPair<int32, FCardPresentationOwnershipEntry>& Pair :
		CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.BattleId != BattleId
			|| Entry.RuntimeId == INDEX_NONE
			|| !DisplayedHandRuntimeIds.Contains(Entry.RuntimeId))
		{
			EntriesToClear.Add(Pair.Key);
			continue;
		}

		// Hand restoration is a degradation path only for a still-visible
		// SelectionArea owner. An accepted Transition/ConsumedPendingReducer owner
		// must be terminated by its explicit presentation/reducer recovery path.
		if (Entry.Owner == ECardPresentationOwner::SelectionArea
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
	PublishCardPresentationOwnershipChanged(EntriesToClear);
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
		return Watermark.StateRevision > 0
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
