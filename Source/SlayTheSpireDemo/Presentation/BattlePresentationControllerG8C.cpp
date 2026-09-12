#include "BattlePresentationController.h"

#include "PresentationDamageReducer.h"
#include "PresentationDamageTiming.h"
#include "../UI/BattleHUDViewModel.h"
#include "../UI/BattleHUDWidgetBase.h"

void UBattlePresentationController::SetDetachedDamageG8CEnabled(bool bEnabled)
{
	if (bDetachedDamageG8CEnabled == bEnabled)
	{
		return;
	}

	bDetachedDamageG8CEnabled = bEnabled;
	if (!bEnabled)
	{
		// Feature disable remains a policy change, not an authority replacement.
		// Keep the current SessionToken, retire current-session cosmetics, and
		// refresh only when authoritative chronology is otherwise caught up.
		CancelCurrentSessionDetachedDamageVisuals();
		RefreshInputIfPresentationCaughtUp();
	}
}

UBattlePresentationController::EDetachedDamageAttemptResult
UBattlePresentationController::TryCommitDetachedDamageRecord(
	const FPresentationRecord& Record)
{
	if (!bDetachedDamageG8CEnabled
		|| Record.Type != EBattlePresentationRecordType::Damage
		|| !IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid()
		|| !IsValid(Widget)
		|| !bHasWorkingPresentationSnapshot
		|| !bHasActiveEnvelope
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex)
		|| ActiveEnvelope.FinalStateRevision <= 0)
	{
		return EDetachedDamageAttemptResult::DeclinedToBlocking;
	}

	const int32 ExpectedRecordIndex = ActiveRecordIndex;
	UBattleHUDWidgetBase* const ExpectedWidget = Widget.Get();
	const FPresentationSessionToken ExpectedSession = ActivePresentationSessionToken;

	FPresentationStateSnapshot CandidateSnapshot = WorkingPresentationSnapshot;
	if (!PresentationDamageReducer::TryApplyDamageRecord(
		CandidateSnapshot,
		Record.Damage))
	{
		ReconcileActiveEnvelopeToFinalSnapshot();
		return EDetachedDamageAttemptResult::Consumed;
	}

	// DamageNumber keeps the historical 0.5s value only as a finite cosmetic
	// lifetime. G8-D/G8-E never use this duration as readiness or chronology debt.
	const float DamageNumberVisualDuration =
		PresentationDamageTiming::GetLegacyDamageBlockingDuration();
	if (!FMath::IsFinite(DamageNumberVisualDuration)
		|| DamageNumberVisualDuration <= 0.0f)
	{
		return EDetachedDamageAttemptResult::DeclinedToBlocking;
	}

	FDetachedDamageToken DetachedToken;
	if (!ExpectedWidget->PrepareDetachedDamageVisual(
		ExpectedSession,
		Record,
		ActiveEnvelope.FinalStateRevision,
		DamageNumberVisualDuration,
		DetachedToken))
	{
		return EDetachedDamageAttemptResult::DeclinedToBlocking;
	}

	if (!bDetachedDamageG8CEnabled
		|| !IsDetachedDamageCommitContextCurrent(
			Record,
			ExpectedRecordIndex,
			ExpectedWidget,
			ExpectedSession))
	{
		ExpectedWidget->CancelDetachedDamageVisual(DetachedToken);
		return EDetachedDamageAttemptResult::Consumed;
	}

	// Formal commit has no compatibility-wait bookkeeping. Publication may
	// synchronously trigger replacement/disablement; after this boundary the
	// reducer is committed exactly once and must never fall back/replay.
	WorkingPresentationSnapshot = MoveTemp(CandidateSnapshot);

	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	}

	if (!IsDetachedDamageCommitContextCurrent(
		Record,
		ExpectedRecordIndex,
		ExpectedWidget,
		ExpectedSession))
	{
		ExpectedWidget->CancelDetachedDamageVisual(DetachedToken);
		return EDetachedDamageAttemptResult::Consumed;
	}

	if (bDetachedDamageG8CEnabled)
	{
		if (!ExpectedWidget->ActivatePreparedDetachedDamageVisual(DetachedToken))
		{
			ExpectedWidget->CancelDetachedDamageVisual(DetachedToken);
		}

		ExpectedWidget->PlayCommittedDamageCombatantCues(Record, ExpectedSession);
	}
	else
	{
		ExpectedWidget->CancelDetachedDamageVisual(DetachedToken);
	}

	AdvancePastCommittedDetachedDamageRecord();
	return EDetachedDamageAttemptResult::Consumed;
}

bool UBattlePresentationController::IsDetachedDamageCommitContextCurrent(
	const FPresentationRecord& Record,
	int32 ExpectedRecordIndex,
	UBattleHUDWidgetBase* ExpectedWidget,
	const FPresentationSessionToken& ExpectedSession) const
{
	if (!bHasActiveEnvelope
		|| !bHasWorkingPresentationSnapshot
		|| ExpectedRecordIndex < 0
		|| ActiveRecordIndex != ExpectedRecordIndex
		|| !ActiveEnvelope.Records.IsValidIndex(ExpectedRecordIndex)
		|| Widget.Get() != ExpectedWidget
		|| !IsValid(ExpectedWidget)
		|| !IsCurrentPresentationSession(ExpectedSession))
	{
		return false;
	}

	const FPresentationRecord& CurrentRecord =
		ActiveEnvelope.Records[ExpectedRecordIndex];
	return CurrentRecord.Type == EBattlePresentationRecordType::Damage
		&& CurrentRecord.BattleId == Record.BattleId
		&& CurrentRecord.ResolutionId == Record.ResolutionId
		&& CurrentRecord.PresentationSequence == Record.PresentationSequence;
}

void UBattlePresentationController::AdvancePastCommittedDetachedDamageRecord()
{
	if (!bHasActiveEnvelope
		|| !ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		return;
	}

	++ActiveRecordIndex;
	if (ActiveEnvelope.Records.IsValidIndex(ActiveRecordIndex))
	{
		StartNextRecord();
		return;
	}
	CompleteActiveEnvelope();
}

void UBattlePresentationController::RefreshInputIfPresentationCaughtUp()
{
	if (!IsValid(ViewModel)
		|| !IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid()
		|| bHasActiveEnvelope
		|| PlaybackQueue.Num() > 0
		|| bWaitingForCompletion)
	{
		return;
	}

	if (ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->InteractionState == EBattleHUDInteractionState::Terminal
		|| ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		return;
	}

	// ViewModel owns exact frozen/read-facing revision checks. G8-E adds no
	// presentation-side timing barrier after chronological work has completed.
	ViewModel->RefreshLiveInputBindingsIfCaughtUp();
}

void UBattlePresentationController::CancelCurrentSessionDetachedDamageVisuals()
{
	if (ActivePresentationSessionToken.IsValid() && IsValid(Widget))
	{
		Widget->CancelDetachedDamageVisualsForSession(
			ActivePresentationSessionToken);
	}
}
