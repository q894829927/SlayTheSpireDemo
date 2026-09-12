#include "BattlePresentationController.h"

#include "PresentationDamageReducer.h"
#include "PresentationDamageTiming.h"
#include "../Battle/BattleManager.h"
#include "../UI/BattleHUDViewModel.h"
#include "../UI/BattleHUDWidgetBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UBattlePresentationController::SetDetachedDamageG8CEnabled(bool bEnabled)
{
	if (bDetachedDamageG8CEnabled == bEnabled)
	{
		return;
	}

	bDetachedDamageG8CEnabled = bEnabled;
	if (!bEnabled)
	{
		// G8-D keeps feature disable as a policy change, not an authority
		// replacement. Retire current-session cosmetics, keep SessionToken, and
		// only refresh input when chronology is otherwise caught up.
		ClearCompatibilityDebt();
		CancelCurrentSessionDetachedDamageVisuals();
		TryServiceCompatibilityDebtOrRefreshInput();
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

	// G8-D still uses the existing finite DamageNumber duration as cosmetic
	// lifetime. It no longer contributes to readiness, FastInput or chronology.
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

	// G8-D formal commit has no compatibility-debt bookkeeping. Publication may
	// synchronously trigger replacement/disablement; after this point the reducer
	// is committed exactly once and must never fall back/replay.
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

// -----------------------------------------------------------------------------
// G8-C compatibility-debt migration shims.
//
// These helpers remain temporarily because G0-G8 call sites already route
// through them, but G8-D never accrues or services debt. This guarantees that
// readiness and FastInput are independent from DamageNumber lifetime while
// keeping the migration patch narrow. G8-E may physically remove these shims.
// -----------------------------------------------------------------------------

void UBattlePresentationController::AddCompatibilityDebtForCommittedDamage(
	float /*DurationSeconds*/)
{
	ClearCompatibilityDebt();
}

void UBattlePresentationController::PauseCompatibilityDebtService()
{
	ClearCompatibilityDebt();
}

bool UBattlePresentationController::IsExactReadSurfaceCaughtUpForDebtService() const
{
	return false;
}

void UBattlePresentationController::TryServiceCompatibilityDebtOrRefreshInput()
{
	ClearCompatibilityDebt();

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

	// ViewModel owns the exact frozen/read-facing revision checks. In G8-D there
	// is no extra Damage timing barrier once chronological work has completed.
	ViewModel->RefreshLiveInputBindingsIfCaughtUp();
}

void UBattlePresentationController::HandleCompatibilityDebtElapsed()
{
	ClearCompatibilityDebt();
	TryServiceCompatibilityDebtOrRefreshInput();
}

void UBattlePresentationController::ClearCompatibilityDebt()
{
	ABattleManager* Battle = BattleManager.Get();
	UWorld* World = IsValid(Battle) ? Battle->GetWorld() : nullptr;
	if (IsValid(World) && CompatibilityDebtTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(CompatibilityDebtTimerHandle);
	}
	CompatibilityDebtTimerHandle.Invalidate();
	CompatibilityDebtSeconds = 0.0f;
}

void UBattlePresentationController::CancelCurrentSessionDetachedDamageVisuals()
{
	if (ActivePresentationSessionToken.IsValid() && IsValid(Widget))
	{
		Widget->CancelDetachedDamageVisualsForSession(
			ActivePresentationSessionToken);
	}
}

bool UBattlePresentationController::HasCompatibilityDebt() const
{
	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UBattlePresentationController::IsCompatibilityDebtServiceActiveForTesting() const
{
	return false;
}
#endif
