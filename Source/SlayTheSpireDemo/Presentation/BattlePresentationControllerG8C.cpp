#include "BattlePresentationController.h"

#include "PresentationDamageReducer.h"
#include "PresentationDamageTiming.h"
#include "../Battle/BattleManager.h"
#include "../Battle/BattleReadSnapshot.h"
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
		// Feature disable is not Presentation authority replacement. Keep the
		// current SessionToken, but synchronously retire staging-only state.
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

	// Formal historical validation/reduction is single-sourced. Invalid Damage is
	// an active-envelope historical failure, not a reason to show an old Blocking
	// animation against a malformed record.
	FPresentationStateSnapshot CandidateSnapshot = WorkingPresentationSnapshot;
	if (!PresentationDamageReducer::TryApplyDamageRecord(
		CandidateSnapshot,
		Record.Damage))
	{
		ReconcileActiveEnvelopeToFinalSnapshot();
		return EDetachedDamageAttemptResult::Consumed;
	}

	const float LegacyDuration =
		PresentationDamageTiming::GetLegacyDamageBlockingDuration();
	if (!FMath::IsFinite(LegacyDuration) || LegacyDuration <= 0.0f)
	{
		return EDetachedDamageAttemptResult::DeclinedToBlocking;
	}

	FDetachedDamageToken DetachedToken;
	if (!ExpectedWidget->PrepareDetachedDamageVisual(
		ExpectedSession,
		Record,
		ActiveEnvelope.FinalStateRevision,
		LegacyDuration,
		DetachedToken))
	{
		// Eligibility/geometry/capacity decline is the authorized fallback to the
		// existing G0-G7 Blocking Damage path. No formal state changed yet.
		return EDetachedDamageAttemptResult::DeclinedToBlocking;
	}

	// Hidden prepare must not give a stale owner/session permission to commit.
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

	// Formal commit happens before any public ViewModel publication. G8-C debt is
	// added after the formal snapshot commit but before publication so a
	// synchronous Skip observer can see and clear it, and the transaction cannot
	// accidentally re-add skipped debt afterwards.
	WorkingPresentationSnapshot = MoveTemp(CandidateSnapshot);
	AddCompatibilityDebtForCommittedDamage(LegacyDuration);

	if (IsValid(ViewModel))
	{
		ViewModel->ApplyPresentationSnapshot(WorkingPresentationSnapshot, true);
	}

	// Publication may synchronously cause Skip/replacement/disablement. Formal
	// reduction is already committed; never replay it and never fall back to the
	// old Blocking path after this boundary.
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

		// Combatant Hit/Attack cues are separate best-effort committed visuals.
		// A DamageNumber activation failure must not suppress them or roll back the
		// already committed reducer state.
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

void UBattlePresentationController::AddCompatibilityDebtForCommittedDamage(
	float DurationSeconds)
{
	if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		return;
	}

	const float NewDebt = CompatibilityDebtSeconds + DurationSeconds;
	CompatibilityDebtSeconds = FMath::IsFinite(NewDebt)
		? NewDebt
		: TNumericLimits<float>::Max();
}

void UBattlePresentationController::PauseCompatibilityDebtService()
{
	if (!CompatibilityDebtTimerHandle.IsValid())
	{
		return;
	}

	ABattleManager* Battle = BattleManager.Get();
	UWorld* World = IsValid(Battle) ? Battle->GetWorld() : nullptr;
	if (IsValid(World))
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		const float Remaining = TimerManager.GetTimerRemaining(
			CompatibilityDebtTimerHandle);
		if (FMath::IsFinite(Remaining) && Remaining >= 0.0f)
		{
			CompatibilityDebtSeconds = Remaining;
		}
		TimerManager.ClearTimer(CompatibilityDebtTimerHandle);
	}
	CompatibilityDebtTimerHandle.Invalidate();
}

bool UBattlePresentationController::IsExactReadSurfaceCaughtUpForDebtService() const
{
	ABattleManager* Battle = BattleManager.Get();
	if (!IsValid(Battle)
		|| !IsValid(ViewModel)
		|| !IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid())
	{
		return false;
	}

	FPresentationStateSnapshot LatestBaseline;
	FBattleReadSnapshot CurrentRead;
	return Battle->TryGetLatestFrozenPresentationBaseline(LatestBaseline)
		&& LatestBaseline.BattleId == CurrentBattleId
		&& LatestBaseline.BattleId == ViewModel->BattleId
		&& LatestBaseline.StateRevision == ViewModel->StateRevision
		&& Battle->TryBuildPlayerFacingReadSnapshot(CurrentRead)
		&& static_cast<int64>(CurrentRead.BattleId) == ViewModel->BattleId
		&& static_cast<int64>(CurrentRead.StateRevision) == ViewModel->StateRevision;
}

void UBattlePresentationController::TryServiceCompatibilityDebtOrRefreshInput()
{
	if (!IsValid(ViewModel))
	{
		return;
	}

	// Terminal/unavailable surfaces never owe a staging wait. They are already
	// input-closed for a stronger reason.
	if (ViewModel->Outcome != EBattleHUDOutcome::None
		|| ViewModel->InteractionState == EBattleHUDInteractionState::Terminal
		|| ViewModel->InteractionState == EBattleHUDInteractionState::PresentationUnavailable)
	{
		ClearCompatibilityDebt();
		return;
	}

	if (!IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid())
	{
		ClearCompatibilityDebt();
		return;
	}

	// Debt is serviced only at an otherwise-ready chronological/read boundary.
	// Other Blocking playback and undelivered newer read edges cannot consume it.
	if (bHasActiveEnvelope
		|| PlaybackQueue.Num() > 0
		|| bWaitingForCompletion
		|| !IsExactReadSurfaceCaughtUpForDebtService())
	{
		return;
	}

	if (!bDetachedDamageG8CEnabled)
	{
		ClearCompatibilityDebt();
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
		return;
	}

	if (!FMath::IsFinite(CompatibilityDebtSeconds)
		|| CompatibilityDebtSeconds <= KINDA_SMALL_NUMBER)
	{
		ClearCompatibilityDebt();
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
		return;
	}

	if (CompatibilityDebtTimerHandle.IsValid())
	{
		return;
	}

	UWorld* World = BattleManager.IsValid() ? BattleManager->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[Presentation][G8-C] Compatibility debt cannot use the battle World timer; entering PresentationUnavailable fail-safe."));
		EnterPresentationUnavailableFailSafe();
		return;
	}

	// FTimerManager uses the same game-time/pause/time-dilation domain as the
	// legacy Native Blocking Damage timer. No platform/wall-clock source is used.
	World->GetTimerManager().SetTimer(
		CompatibilityDebtTimerHandle,
		this,
		&UBattlePresentationController::HandleCompatibilityDebtElapsed,
		CompatibilityDebtSeconds,
		false);
}

void UBattlePresentationController::HandleCompatibilityDebtElapsed()
{
	CompatibilityDebtTimerHandle.Invalidate();
	CompatibilityDebtSeconds = 0.0f;

	if (!bDetachedDamageG8CEnabled
		|| !IsPresentationOwnedMode()
		|| !ActivePresentationSessionToken.IsValid()
		|| bHasActiveEnvelope
		|| PlaybackQueue.Num() > 0
		|| bWaitingForCompletion
		|| !IsExactReadSurfaceCaughtUpForDebtService())
	{
		return;
	}

	if (IsValid(ViewModel))
	{
		ViewModel->RefreshLiveInputBindingsIfCaughtUp();
	}
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
	return CompatibilityDebtSeconds > KINDA_SMALL_NUMBER
		|| CompatibilityDebtTimerHandle.IsValid();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UBattlePresentationController::IsCompatibilityDebtServiceActiveForTesting() const
{
	if (!CompatibilityDebtTimerHandle.IsValid())
	{
		return false;
	}
	ABattleManager* Battle = BattleManager.Get();
	UWorld* World = IsValid(Battle) ? Battle->GetWorld() : nullptr;
	return IsValid(World)
		&& World->GetTimerManager().IsTimerActive(CompatibilityDebtTimerHandle);
}
#endif
