#include "BattlePresentationController.h"
#include "../UI/BattleHUDWidgetBase.h"
#include "../UI/BattleHUDViewModel.h"
#include "UObject/StrongObjectPtr.h"

void UBattlePresentationController::SetDetachedCardDestinationD1Enabled(bool bEnabled)
{
	if (bDetachedCardDestinationD1Enabled == bEnabled) return;
	bDetachedCardDestinationD1Enabled = bEnabled;
	if (!bEnabled && IsValid(Widget))
	{
		Widget->DiscardQueuedPlayerInput();
		Widget->CancelDetachedCardDestinationVisuals();
		RefreshInputIfPresentationCaughtUp();
	}
}

UBattlePresentationController::EDetachedRecordAttemptResult
UBattlePresentationController::TryCommitDetachedCardDestinationRecord(const FPresentationRecord& OfferedRecord)
{
	if (!bDetachedCardDestinationD1Enabled || !IsValid(Widget) || !bHasWorkingPresentationSnapshot
		|| !IsPresentationOwnedMode() || !ActivePresentationSessionToken.IsValid())
		return EDetachedRecordAttemptResult::DeclinedToBlocking;

	// A publication/prepare hook may replace and free ActiveEnvelope. Never keep
	// a reference into its Records across either external boundary.
	const FPresentationRecord Record = OfferedRecord;
	const TStrongObjectPtr<UBattlePresentationController> KeepController(this);
	const TStrongObjectPtr<UBattleHUDWidgetBase> KeepOwner(Widget.Get());
	const int32 Index = ActiveRecordIndex;
	UBattleHUDWidgetBase* const Owner = Widget.Get();
	const auto Session = ActivePresentationSessionToken;
	auto Candidate = WorkingPresentationSnapshot;
	auto CandidateHistory = CardHistoryState;
	FPlayedCardPresentationLifecycleToken Lifecycle;
	if (!PresentationCardReducer::TryApplyRecord(Candidate, CandidateHistory, Record, Session, &Lifecycle)
		|| !Lifecycle.IsValid())
	{
		ReconcileActiveEnvelopeToFinalSnapshot();
		return EDetachedRecordAttemptResult::Consumed;
	}
	const auto Zone = Record.CardZoneChanged.ToZone;
	if (Zone != ECardZone::DiscardPile && Zone != ECardZone::ExhaustPile && Zone != ECardZone::RemovedPile)
		return EDetachedRecordAttemptResult::DeclinedToBlocking;

	FDetachedCardDestinationToken Receipt;
	const bool bPrepared = Owner->PrepareDetachedCardDestination(Record, Lifecycle, Receipt);
	if (!IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session))
	{
		Owner->CancelPreparedDetachedCardDestination(Receipt);
		return EDetachedRecordAttemptResult::Consumed;
	}
	if (!bPrepared || !bDetachedCardDestinationD1Enabled)
	{
		Owner->CancelPreparedDetachedCardDestination(Receipt);
		return EDetachedRecordAttemptResult::DeclinedToBlocking;
	}

	// Install both formal candidates before publication, including consuming the
	// unresolved occurrence exactly once. No post-commit path may replay it.
	WorkingPresentationSnapshot = MoveTemp(Candidate);
	CardHistoryState = MoveTemp(CandidateHistory);
	CommittedCardDestinationReceipt = Receipt;
	if (IsValid(ViewModel)) ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	if (!IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session))
	{
		Owner->RetireDetachedCardDestination(Receipt);
		if (CommittedCardDestinationReceipt == Receipt) CommittedCardDestinationReceipt = {};
		return EDetachedRecordAttemptResult::Consumed;
	}
	if (!bDetachedCardDestinationD1Enabled || !Owner->ActivatePreparedDetachedCardDestination(Receipt))
		Owner->RetireDetachedCardDestination(Receipt);
	// Activation is another overridable visual boundary; it may recover/replace
	// the owner. Advance only the still-current committed cursor.
	if (CommittedCardDestinationReceipt == Receipt) CommittedCardDestinationReceipt = {};
	if (IsDetachedRecordCommitContextCurrent(Record, Index, Owner, Session)) AdvancePastCommittedDetachedRecord();
	return EDetachedRecordAttemptResult::Consumed;
}

void UBattlePresentationController::CancelCurrentSessionPlayedCardVisuals()
{
	CommittedCardDestinationReceipt = {};
	CommittedCardArrivalReceipt = {}; CommittedArrivalDestinationReceipt = {};
	if (ActivePresentationSessionToken.IsValid() && IsValid(Widget))
		Widget->CancelPlayedCardVisualsForSession(ActivePresentationSessionToken);
}

bool UBattlePresentationController::IsCommittedCardDestinationCurrent(const FDetachedCardDestinationToken& Token) const
{
	if (!Token.IsValid() || !(CommittedCardDestinationReceipt == Token) || !bHasActiveEnvelope
		|| !bHasWorkingPresentationSnapshot || !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| !IsCurrentPresentationSession(Token.Visual.Lifecycle.SessionToken)) return false;
	const auto& Record = ActiveEnvelope.Records[ActiveRecordIndex];
	return Record.Type == EBattlePresentationRecordType::CardZoneChanged && Record.CardZoneChanged.FromZone == ECardZone::PlayArea
		&& Record.BattleId == Token.Visual.Lifecycle.BattleId && Record.ResolutionId == Token.ResolutionId
		&& Record.PresentationSequence == Token.PresentationSequence;
}
