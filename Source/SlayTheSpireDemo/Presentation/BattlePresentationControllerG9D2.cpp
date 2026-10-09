#include "BattlePresentationController.h"
#include "../UI/BattleHUDWidgetBase.h"
#include "../UI/BattleHUDViewModel.h"
#include "UObject/StrongObjectPtr.h"

void UBattlePresentationController::SetDetachedCardArrivalD2Enabled(bool bEnabled)
{
	if (bDetachedCardArrivalD2Enabled == bEnabled) return;
	bDetachedCardArrivalD2Enabled = bEnabled;
	if (!bEnabled && IsValid(Widget))
	{
		Widget->DiscardQueuedPlayerInput();
		Widget->CancelDetachedCardArrivalVisuals();
		RefreshInputIfPresentationCaughtUp();
	}
}

bool UBattlePresentationController::TryGetDetachedCardArrivalPlan(const FPresentationRecord& Record,
	FPlayedCardPresentationLifecycleToken& OutLifecycle, FPresentationRecord& OutDestination) const
{
	OutLifecycle = {}; OutDestination = {};
	FPlayedCardPresentationLifecycleToken Life;
	if (Record.Type != EBattlePresentationRecordType::CardPlayed || !IsPresentationOwnedMode()
		|| !TryGetPlayedCardLifecycleForRecord(Record, Life)) return false;
	auto Snapshot = WorkingPresentationSnapshot; auto History = CardHistoryState;
	if (!PresentationCardReducer::TryApplyRecord(Snapshot, History, Record, ActivePresentationSessionToken)) return false;
	int32 Matches = 0; int64 Sequence = Record.PresentationSequence;
	FPresentationRecord Destination;
	for (int32 I = ActiveRecordIndex+1; I < ActiveEnvelope.Records.Num(); ++I)
	{
		const auto& Next = ActiveEnvelope.Records[I];
		if (Next.BattleId != Record.BattleId || Next.ResolutionId != Record.ResolutionId || Next.PresentationSequence <= Sequence) return false;
		Sequence = Next.PresentationSequence;
		if (Next.Type == EBattlePresentationRecordType::CardZoneChanged && Next.CardZoneChanged.FromZone == ECardZone::PlayArea)
		{
			FPlayedCardPresentationLifecycleToken Consumed;
			if (!PresentationCardReducer::TryApplyRecord(Snapshot, History, Next, ActivePresentationSessionToken, &Consumed)) return false;
			if (Consumed == Life)
			{
				if (Next.Group.IsValid()) return false;
				if (++Matches != 1) return false;
				Destination = Next;
			}
		}
		else if (!ApplyRecordToSnapshot(Snapshot, History, Next, ActiveEnvelope.FinalSnapshot, ActivePresentationSessionToken)) return false;
	}
	if (Matches != 1) return false;
	OutLifecycle = Life; OutDestination = MoveTemp(Destination); return true;
}

UBattlePresentationController::EDetachedRecordAttemptResult
UBattlePresentationController::TryCommitDetachedCardArrivalRecord(const FPresentationRecord& Offered)
{
	if (!bDetachedCardArrivalD2Enabled || !bDetachedCardDestinationD1Enabled || !IsValid(Widget)
		|| !bHasWorkingPresentationSnapshot || DetachedCardArrivalReceipts.Num() >= 32)
		return EDetachedRecordAttemptResult::DeclinedToBlocking;
	const FPresentationRecord Record = Offered;
	const TStrongObjectPtr<UBattlePresentationController> KeepController(this);
	const TStrongObjectPtr<UBattleHUDWidgetBase> KeepOwner(Widget.Get());
	auto* const Owner = Widget.Get(); const auto Session = ActivePresentationSessionToken; const int32 Index = ActiveRecordIndex;
	auto Candidate = WorkingPresentationSnapshot; auto History = CardHistoryState;
	FPlayedCardPresentationLifecycleToken Life;
	if (!PresentationCardReducer::TryApplyRecord(Candidate, History, Record, Session, &Life))
	{ ReconcileActiveEnvelopeToFinalSnapshot(); return EDetachedRecordAttemptResult::Consumed; }
	FPlayedCardPresentationLifecycleToken PlannedLife; FPresentationRecord Destination;
	if (!TryGetDetachedCardArrivalPlan(Record, PlannedLife, Destination) || PlannedLife != Life)
		return EDetachedRecordAttemptResult::DeclinedToBlocking;
	FDetachedCardArrivalToken Receipt;
	const bool Prepared = Owner->PrepareDetachedCardArrival(Record, Destination, Life, Receipt);
	if (!IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session))
	{ Owner->RetireDetachedCardArrival(Receipt); return EDetachedRecordAttemptResult::Consumed; }
	if (!Prepared || !bDetachedCardArrivalD2Enabled || !bDetachedCardDestinationD1Enabled)
	{ Owner->RetireDetachedCardArrival(Receipt); return EDetachedRecordAttemptResult::DeclinedToBlocking; }
	WorkingPresentationSnapshot = MoveTemp(Candidate); CardHistoryState = MoveTemp(History);
	DetachedCardArrivalReceipts.Add(Receipt); CommittedCardArrivalReceipt = Receipt;
	if (IsValid(ViewModel)) ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	if (!IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session))
	{
		Owner->RetireDetachedCardArrival(Receipt);
		if (CommittedCardArrivalReceipt == Receipt) CommittedCardArrivalReceipt = {};
		return EDetachedRecordAttemptResult::Consumed;
	}
	if (!bDetachedCardArrivalD2Enabled || !bDetachedCardDestinationD1Enabled || !Owner->ActivatePreparedDetachedCardArrival(Receipt))
		Owner->RetireDetachedCardArrival(Receipt);
	if (CommittedCardArrivalReceipt == Receipt) CommittedCardArrivalReceipt = {};
	if (IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session)) AdvancePastCommittedDetachedRecord();
	return EDetachedRecordAttemptResult::Consumed;
}

UBattlePresentationController::EDetachedRecordAttemptResult
UBattlePresentationController::TryCommitDetachedArrivalDestinationRecord(const FPresentationRecord& Offered)
{
	if (!IsPresentationOwnedMode() || !bHasWorkingPresentationSnapshot || !IsValid(Widget))
		return EDetachedRecordAttemptResult::DeclinedToBlocking;
	const FPresentationRecord Record = Offered;
	auto Candidate = WorkingPresentationSnapshot; auto History = CardHistoryState;
	FPlayedCardPresentationLifecycleToken Life;
	if (!PresentationCardReducer::TryApplyRecord(Candidate, History, Record, ActivePresentationSessionToken, &Life))
	{ ReconcileActiveEnvelopeToFinalSnapshot(); return EDetachedRecordAttemptResult::Consumed; }
	const int32 ReceiptIndex = DetachedCardArrivalReceipts.IndexOfByPredicate([&](const auto& R) { return R.Visual.Lifecycle == Life; });
	if (ReceiptIndex == INDEX_NONE) return EDetachedRecordAttemptResult::DeclinedToBlocking;
	const auto Receipt = DetachedCardArrivalReceipts[ReceiptIndex];
	if (Record.ResolutionId != Receipt.DestinationResolutionId || Record.PresentationSequence != Receipt.DestinationPresentationSequence)
	{ ReconcileActiveEnvelopeToFinalSnapshot(); return EDetachedRecordAttemptResult::Consumed; }
	const TStrongObjectPtr<UBattlePresentationController> KeepController(this);
	const TStrongObjectPtr<UBattleHUDWidgetBase> KeepOwner(Widget.Get());
	auto* const Owner = Widget.Get(); const auto Session = ActivePresentationSessionToken; const int32 Index = ActiveRecordIndex;
	WorkingPresentationSnapshot = MoveTemp(Candidate); CardHistoryState = MoveTemp(History);
	DetachedCardArrivalReceipts.RemoveAt(ReceiptIndex); CommittedArrivalDestinationReceipt = Receipt;
	if (IsValid(ViewModel)) ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	if (!IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session))
	{
		Owner->RetireDetachedCardArrival(Receipt);
		if (CommittedArrivalDestinationReceipt == Receipt) CommittedArrivalDestinationReceipt = {};
		return EDetachedRecordAttemptResult::Consumed;
	}
	// A committed arrival's destination is an obligation even after feature disable
	// or visual loss. It cannot return to Blocking or replay the reducer.
	if (!Owner->CommitDetachedCardArrivalDestination(Receipt)) Owner->RetireDetachedCardArrival(Receipt);
	if (CommittedArrivalDestinationReceipt == Receipt) CommittedArrivalDestinationReceipt = {};
	if (IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session)) AdvancePastCommittedDetachedRecord();
	return EDetachedRecordAttemptResult::Consumed;
}

bool UBattlePresentationController::IsCommittedCardArrivalCurrent(const FDetachedCardArrivalToken& Token, bool bDestination) const
{
	if (!Token.IsValid() || !(Token == (bDestination ? CommittedArrivalDestinationReceipt : CommittedCardArrivalReceipt))
		|| !bHasActiveEnvelope || !bHasWorkingPresentationSnapshot || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| !IsCurrentPresentationSession(Token.Visual.Lifecycle.SessionToken)) return false;
	const auto& Record = ActiveEnvelope.Records[ActiveRecordIndex];
	return Record.BattleId == Token.Visual.Lifecycle.BattleId
		&& Record.ResolutionId == (bDestination ? Token.DestinationResolutionId : Token.Visual.Lifecycle.SourceResolutionId)
		&& Record.PresentationSequence == (bDestination ? Token.DestinationPresentationSequence : Token.Visual.Lifecycle.CardPlayedPresentationSequence)
		&& Record.Type == (bDestination ? EBattlePresentationRecordType::CardZoneChanged : EBattlePresentationRecordType::CardPlayed);
}
