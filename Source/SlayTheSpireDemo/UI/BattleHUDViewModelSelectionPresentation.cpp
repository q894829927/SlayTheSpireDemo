#include "BattleHUDViewModel.h"

#include "../Battle/BattleManager.h"
#include "../Battle/BattleSelectionRequest.h"

bool UBattleHUDViewModel::ConfirmPendingCardSelectionWithPresentation(int64 Generation)
{
	if (bSelectionPresentationSubmitInProgress || !CanConfirmPendingCardSelection()
		|| Generation <= 0 || Generation != ActiveCardPresentationSelectionGeneration)
	{
		return false;
	}
	const TArray<int32> SelectedIds = PendingCardSelectionRuntimeIds;
	int32 EntryCount = 0;
	for (const auto& Pair : CardPresentationOwnershipEntries)
	{
		const FCardPresentationOwnershipEntry& Entry = Pair.Value;
		if (Entry.SelectionGeneration != Generation) continue;
		if (Entry.BattleId != BattleId || Entry.SelectionBoundaryRevision != StateRevision
			|| Entry.Phase != ESelectionPresentationVisualPhase::Pending
			|| Entry.Owner != ECardPresentationOwner::SelectionArea || !SelectedIds.Contains(Pair.Key)) return false;
		++EntryCount;
	}
	if (EntryCount != SelectedIds.Num()) return false;

	ABattleManager* SubmittedBattle = BattleManager.Get();
	const int64 SubmittedBattleId = BattleId;
	SubmittingSelectionBoundary = StateRevision;
	bSelectionPresentationSubmitInProgress = true;
	DeferredSelectionReceipts.Reset();
	DeferredSelectionReadBattleId = DeferredSelectionReadRevision = 0;
	const bool bAccepted = SubmitPendingCardSelectionByRuntimeIds(SelectedIds);
	bSelectionPresentationSubmitInProgress = false;
	const bool bSameBattle = SubmittedBattle == BattleManager.Get() && SubmittedBattleId == BattleId;
	if (bAccepted && bSameBattle)
	{
		// Snapshot/read notifications were held during submit; G0's irreversible
		// Confirm therefore happens only after acceptance on the exact old boundary.
		if (!ConfirmCardPresentationSelection(Generation, SelectedIds))
		{
			CancelCardPresentationSelectionLifecycle(Generation);
			bSelectionCorrelationFailed = true;
			EnterPresentationUnavailable(NSLOCTEXT("BattleHUD", "SelectionConfirmLost", "Selection presentation transaction could not be committed."));
		}
	}
	else if (bSameBattle)
	{
		FPendingCardSelectionReadView Pending;
		if (!TryGetPendingCardSelectionReadView(Pending))
		{
			CancelCardPresentationSelectionLifecycle(Generation);
			ClearPendingCardSelectionInputState();
		}
	}

	const TArray<FSelectionPresentationOutcomeReceipt> Receipts = MoveTemp(DeferredSelectionReceipts);
	for (const auto& Receipt : Receipts) ApplySelectionOutcomeReceipt(Receipt);
	SubmittingSelectionBoundary = 0;
	if (bHasDeferredSelectionSnapshot)
	{
		const FPresentationStateSnapshot Snapshot = DeferredSelectionSnapshot;
		const bool bReset = bDeferredSelectionResetInteraction;
		bHasDeferredSelectionSnapshot = false;
		bDeferredSelectionResetInteraction = false;
		DeferredSelectionSnapshot = FPresentationStateSnapshot{};
		ApplyPresentationSnapshot(Snapshot, bReset);
	}
	if (DeferredSelectionReadBattleId != 0)
	{
		const uint64 ReadBattle = DeferredSelectionReadBattleId;
		const uint64 ReadRevision = DeferredSelectionReadRevision;
		DeferredSelectionReadBattleId = DeferredSelectionReadRevision = 0;
		HandleReadStateReady(ReadBattle, ReadRevision);
	}
	return bAccepted;
}

void UBattleHUDViewModel::AcceptSelectionPresentationOutcomes(const FPresentationResolutionEnvelope& Envelope)
{
	if (Envelope.BattleId != BattleId) return;
	for (const auto& Receipt : Envelope.SelectionOutcomes)
	{
		if (!Receipt.IsValid() || Receipt.BattleId != Envelope.BattleId
			|| Receipt.Mode != ESelectionPresentationOutcomeMode::RecordedResolution
			|| Receipt.ResolutionId != Envelope.ResolutionId) continue;
		if (bSelectionPresentationSubmitInProgress)
		{
			if (Receipt.SelectionBoundaryRevision == SubmittingSelectionBoundary) DeferredSelectionReceipts.Add(Receipt);
		}
		else ApplySelectionOutcomeReceipt(Receipt);
	}
}

void UBattleHUDViewModel::ApplySelectionOutcomeReceipt(const FSelectionPresentationOutcomeReceipt& Receipt)
{
	if (!Receipt.IsValid() || Receipt.BattleId != BattleId) return;
	TArray<int64> Generations;
	for (const auto& Pair : CardPresentationOwnershipEntries)
	{
		const auto& Entry = Pair.Value;
		if (Entry.BattleId == Receipt.BattleId && Entry.SelectionBoundaryRevision == Receipt.SelectionBoundaryRevision
			&& Entry.Phase == ESelectionPresentationVisualPhase::Confirmed) Generations.AddUnique(Entry.SelectionGeneration);
	}
	Generations.Sort();
	for (int64 Generation : Generations)
	{
		if (Receipt.Mode == ESelectionPresentationOutcomeMode::RecordedResolution)
			ArmRecordedCardPresentationCompletion(Generation, Receipt.ResolutionId);
		else if (Receipt.Mode == ESelectionPresentationOutcomeMode::DirectStateRevision)
			ArmDirectCardPresentationCompletion(Generation, Receipt.StateRevision);
	}
}

void UBattleHUDViewModel::ResolveSelectionPresentationReadEdge(uint64 InBattleId, uint64 InStateRevision)
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle) || static_cast<int64>(InBattleId) != BattleId) return;
	TArray<int64> Boundaries;
	for (const auto& Pair : CardPresentationOwnershipEntries)
	{
		const auto& Entry = Pair.Value;
		if (Entry.Phase == ESelectionPresentationVisualPhase::Confirmed && !Entry.CompletionWatermark.IsResolved())
			Boundaries.AddUnique(Entry.SelectionBoundaryRevision);
	}
	Boundaries.Sort();
	for (int64 Boundary : Boundaries)
	{
		FSelectionPresentationOutcomeReceipt Receipt;
		if (Battle->TryGetDirectSelectionPresentationOutcome(Boundary, Receipt)) ApplySelectionOutcomeReceipt(Receipt);
	}
	TArray<int32> FailedIds;
	for (const auto& Pair : CardPresentationOwnershipEntries)
	{
		const auto& Entry = Pair.Value;
		// Public read delivery follows all sealed outcome envelopes. A newer Ready
		// edge without either receipt is an explicit metadata failure, not a guess
		// that an unrelated Resolution completed this selection.
		if (Entry.Phase == ESelectionPresentationVisualPhase::Confirmed
			&& !Entry.CompletionWatermark.IsResolved()
			&& static_cast<int64>(InStateRevision) > Entry.SelectionBoundaryRevision) FailedIds.Add(Pair.Key);
	}
	if (!FailedIds.IsEmpty())
	{
		for (int32 RuntimeId : FailedIds) CardPresentationOwnershipEntries.Remove(RuntimeId);
		bSelectionCorrelationFailed = true;
		PublishCardPresentationOwnershipChanged(FailedIds);
		EnterPresentationUnavailable(NSLOCTEXT("BattleHUD", "SelectionOutcomeMissing", "Selection presentation outcome is unavailable."));
	}
}
