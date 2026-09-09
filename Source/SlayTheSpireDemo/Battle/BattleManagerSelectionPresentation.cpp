#include "BattleManager.h"

#include "../Actions/BattleActionQueue.h"

bool ABattleManager::RegisterDirectSelectionOutcomeBoundary(
	uint64 InBattleId,
	int64 SelectionBoundaryRevision
)
{
	if (InBattleId == 0
		|| InBattleId != BattleId
		|| SelectionBoundaryRevision <= 0
		|| static_cast<uint64>(SelectionBoundaryRevision) > StateRevision)
	{
		return false;
	}

	if (DirectSelectionOutcomeBattleId != BattleId)
	{
		DirectSelectionOutcomeBattleId = BattleId;
		PendingDirectSelectionOutcomeBoundaryRevisions.Reset();
		DirectSelectionOutcomeReceipts.Reset();
	}

	const bool bAlreadyFinalized = DirectSelectionOutcomeReceipts.ContainsByPredicate(
		[SelectionBoundaryRevision](const FSelectionPresentationOutcomeReceipt& Receipt)
		{
			return Receipt.SelectionBoundaryRevision == SelectionBoundaryRevision;
		}
	);
	if (bAlreadyFinalized)
	{
		return true;
	}

	PendingDirectSelectionOutcomeBoundaryRevisions.AddUnique(SelectionBoundaryRevision);
	return true;
}

void ABattleManager::ResolvePendingDirectSelectionOutcomesForReadEdge()
{
	if (PendingDirectSelectionOutcomeBoundaryRevisions.IsEmpty())
	{
		return;
	}

	if (DirectSelectionOutcomeBattleId != BattleId)
	{
		DirectSelectionOutcomeBattleId = BattleId;
		PendingDirectSelectionOutcomeBoundaryRevisions.Reset();
		DirectSelectionOutcomeReceipts.Reset();
		return;
	}

	int64 MaxBoundaryRevision = 0;
	for (const int64 BoundaryRevision : PendingDirectSelectionOutcomeBoundaryRevisions)
	{
		MaxBoundaryRevision = FMath::Max(MaxBoundaryRevision, BoundaryRevision);
	}

	// A direct outcome receipt must identify a strictly newer authoritative read
	// edge. When Gameplay did not otherwise advance the revision, create exactly
	// one presentation/read revision after the queue has become stable. If another
	// interactive Selection already advanced the revision, that newer boundary is
	// already an exact chronological edge after the earlier continuation work.
	bool bAdvancedRevisionForOutcome = false;
	UBattleActionQueue* Queue = ActionQueue.Get();
	if (StateRevision <= static_cast<uint64>(MaxBoundaryRevision))
	{
		if (IsValid(Queue) && Queue->IsBusy())
		{
			return;
		}
		AdvanceStateRevision();
		bAdvancedRevisionForOutcome = true;
	}

	const int64 OutcomeRevision = static_cast<int64>(StateRevision);
	for (int32 Index = PendingDirectSelectionOutcomeBoundaryRevisions.Num() - 1; Index >= 0; --Index)
	{
		const int64 BoundaryRevision = PendingDirectSelectionOutcomeBoundaryRevisions[Index];
		if (BoundaryRevision <= 0 || OutcomeRevision <= BoundaryRevision)
		{
			continue;
		}

		FSelectionPresentationOutcomeReceipt Receipt;
		Receipt.BattleId = static_cast<int64>(BattleId);
		Receipt.SelectionBoundaryRevision = BoundaryRevision;
		Receipt.Mode = ESelectionPresentationOutcomeMode::DirectStateRevision;
		Receipt.StateRevision = OutcomeRevision;
		if (!Receipt.IsValid())
		{
			continue;
		}

		const bool bAlreadyFinalized = DirectSelectionOutcomeReceipts.ContainsByPredicate(
			[BoundaryRevision](const FSelectionPresentationOutcomeReceipt& Existing)
			{
				return Existing.SelectionBoundaryRevision == BoundaryRevision;
			}
		);
		if (!bAlreadyFinalized)
		{
			DirectSelectionOutcomeReceipts.Add(Receipt);
		}
		PendingDirectSelectionOutcomeBoundaryRevisions.RemoveAt(Index);
	}

	// In no-history mode the ViewModel reads the latest frozen baseline at the
	// Ready edge. If G1 had to create the post-confirm revision itself, refresh
	// that baseline to the same exact target revision before publication.
	if (bAdvancedRevisionForOutcome)
	{
		FreezeLatestPresentationBaselineWithoutResolution();
	}
}

bool ABattleManager::TryGetDirectSelectionPresentationOutcome(
	int64 SelectionBoundaryRevision,
	FSelectionPresentationOutcomeReceipt& OutReceipt
) const
{
	OutReceipt = FSelectionPresentationOutcomeReceipt{};
	if (SelectionBoundaryRevision <= 0 || DirectSelectionOutcomeBattleId != BattleId)
	{
		return false;
	}

	const FSelectionPresentationOutcomeReceipt* Receipt = DirectSelectionOutcomeReceipts.FindByPredicate(
		[SelectionBoundaryRevision](const FSelectionPresentationOutcomeReceipt& Candidate)
		{
			return Candidate.SelectionBoundaryRevision == SelectionBoundaryRevision;
		}
	);
	if (Receipt == nullptr || !Receipt->IsValid())
	{
		return false;
	}

	OutReceipt = *Receipt;
	return true;
}
