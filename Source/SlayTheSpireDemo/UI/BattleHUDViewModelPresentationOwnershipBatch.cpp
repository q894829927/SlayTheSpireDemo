#include "BattleHUDViewModel.h"

namespace
{
	bool IsValidG6OwnershipBatchTransfer(
		ECardPresentationOwner ExpectedOwner,
		ECardPresentationOwner NewOwner)
	{
		return (ExpectedOwner == ECardPresentationOwner::SelectionArea
				&& NewOwner == ECardPresentationOwner::Transition)
			|| (ExpectedOwner == ECardPresentationOwner::Transition
				&& NewOwner == ECardPresentationOwner::ConsumedPendingReducer);
	}
}

bool UBattleHUDViewModel::TryTransferCardPresentationOwnershipBatch(
	int64 SelectionGeneration,
	const TArray<int32>& RuntimeIds,
	ECardPresentationOwner ExpectedOwner,
	ECardPresentationOwner NewOwner)
{
	if (SelectionGeneration <= 0
		|| RuntimeIds.IsEmpty()
		|| !IsValidG6OwnershipBatchTransfer(ExpectedOwner, NewOwner))
	{
		return false;
	}

	TSet<int32> UniqueRuntimeIds;
	UniqueRuntimeIds.Reserve(RuntimeIds.Num());
	for (const int32 RuntimeId : RuntimeIds)
	{
		if (RuntimeId == INDEX_NONE || UniqueRuntimeIds.Contains(RuntimeId))
		{
			return false;
		}

		const FCardPresentationOwnershipEntry* Entry =
			CardPresentationOwnershipEntries.Find(RuntimeId);
		if (Entry == nullptr
			|| Entry->BattleId != BattleId
			|| Entry->SelectionGeneration != SelectionGeneration
			|| Entry->Owner != ExpectedOwner
			|| Entry->Phase != ESelectionPresentationVisualPhase::Confirmed)
		{
			return false;
		}
		UniqueRuntimeIds.Add(RuntimeId);
	}

	// Mutation and publication are intentionally split. Every entry is proven
	// valid above, so observers see either the old cohort or the fully advanced
	// cohort, never a per-member intermediate ownership state.
	for (const int32 RuntimeId : RuntimeIds)
	{
		CardPresentationOwnershipEntries.FindChecked(RuntimeId).Owner = NewOwner;
	}
	PublishCardPresentationOwnershipChanged(RuntimeIds);
	return true;
}
