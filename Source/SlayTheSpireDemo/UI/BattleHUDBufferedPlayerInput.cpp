#include "BattleHUDBufferedPlayerInput.h"

#include "BattleHUDViewModel.h"
#include "BattleHUDWidgetBase.h"
#include "../Battle/BattleManager.h"
#include "../Presentation/BattlePresentationController.h"

void FBattleHUDBufferedPlayerInput::Bind(UBattleHUDWidgetBase* InOwner)
{
	UBattleHUDViewModel* NewViewModel = IsValid(InOwner) ? InOwner->ViewModel.Get() : nullptr;
	ABattleManager* NewBattle = IsValid(NewViewModel) ? NewViewModel->BattleManager.Get() : nullptr;
	UBattlePresentationController* NewController = IsValid(InOwner) ? InOwner->PresentationController.Get() : nullptr;
	if (Owner.Get() != InOwner || ViewModel.Get() != NewViewModel
		|| Battle.Get() != NewBattle || Controller.Get() != NewController)
	{
		Clear();
	}
	Owner = InOwner;
	ViewModel = NewViewModel;
	Battle = NewBattle;
	Controller = NewController;
}

void FBattleHUDBufferedPlayerInput::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	if (!bEnabled) Clear();
}

void FBattleHUDBufferedPlayerInput::Clear()
{
	Pending = FBufferedPlayerIntentDecision{};
}

bool FBattleHUDBufferedPlayerInput::HasSafeBinding() const
{
	return bEnabled && Owner.IsValid() && ViewModel.IsValid() && Battle.IsValid()
		&& Owner->ViewModel == ViewModel.Get()
		&& Owner->PresentationController == Controller.Get()
		&& !Controller.IsStale()
		&& (!Controller.IsValid() || (Controller->BattleManager == Battle
			&& Controller->ViewModel == ViewModel.Get() && Controller->Widget == Owner.Get()))
		&& ViewModel->BattleManager == Battle
		&& Battle->IsPresentationAvailable()
		&& ViewModel->Outcome == EBattleHUDOutcome::None
		&& ViewModel->InteractionState != EBattleHUDInteractionState::Terminal
		&& ViewModel->InteractionState != EBattleHUDInteractionState::PresentationUnavailable;
}

bool FBattleHUDBufferedPlayerInput::TryGetEndTurnAuthority(FEndTurnIntentAvailability& OutAvailability) const
{
	OutAvailability = FEndTurnIntentAvailability{};
	if (!HasSafeBinding() || ViewModel->HasAuthoritativePendingCardSelection()
		|| !Battle->TryGetCurrentPlayerTurnAuthorityToken(OutAvailability.Turn)
		|| ViewModel->BattleId != static_cast<int64>(OutAvailability.Turn.BattleId))
	{
		return false;
	}
	if (ViewModel->IsPresentationDisplayOwned())
	{
		FPresentationSessionToken Session;
		if (!Controller.IsValid() || !Controller->TryGetPresentationSessionToken(Session)
			|| !Controller->IsCurrentPresentationSession(Session)
			|| Session.BattleId != ViewModel->BattleId)
		{
			return false;
		}
	}
	const FGameplayValidationResult Query = Battle->QueryEndPlayerTurn();
	OutAvailability.bCanExecuteEndTurnNow = Query.bAllowed;
	return Query.bAllowed || Query.FailureReason == EGameplayRequestFailureReason::ResolutionBusy;
}

FEndTurnIntentAvailability FBattleHUDBufferedPlayerInput::EvaluateEndTurnAvailability() const
{
	FEndTurnIntentAvailability Result;
	Result.bCanAcceptEndTurnIntent = TryGetEndTurnAuthority(Result);
	Result.bCanAcceptEndTurnIntent &= Pending.Kind != EBufferedPlayerIntentKind::EndTurn;
	return Result;
}

FEndTurnIntentAcceptance FBattleHUDBufferedPlayerInput::TryAcceptEndTurn(bool bHasPendingFastInputRetry)
{
	FEndTurnIntentAcceptance Result;
	const FEndTurnIntentAvailability Availability = EvaluateEndTurnAvailability();
	if (!Availability.bCanAcceptEndTurnIntent) return Result;

	FBufferedEndTurnIntent Intent;
	Intent.Turn = Availability.Turn;
	Intent.CaptureStateRevision = ViewModel->StateRevision;
	Intent.LocalIntentGeneration = NextIntentGeneration++;
	if (NextIntentGeneration == 0) ++NextIntentGeneration;
	if (ViewModel->IsPresentationDisplayOwned())
	{
		FPresentationSessionToken Session;
		if (!Controller->TryGetPresentationSessionToken(Session)) return Result;
		Intent.PresentationFence = Session;
	}

	// No retirement is reported until exact Gameplay authority has been accepted.
	Result.bAccepted = true;
	Result.bRetireCardIntent = Pending.Kind == EBufferedPlayerIntentKind::CardSelection;
	Result.bRetireFastInputRetry = bHasPendingFastInputRetry;
	Result.bCancelTransientSelection = ViewModel->InteractionState == EBattleHUDInteractionState::ReadyToConfirm
		|| ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget;
	Clear();
	Pending.Kind = EBufferedPlayerIntentKind::EndTurn;
	Pending.EndTurn = Intent;
	return Result;
}

bool FBattleHUDBufferedPlayerInput::TryCaptureCard(int32 RuntimeId)
{
	if (!HasSafeBinding() || Pending.Kind == EBufferedPlayerIntentKind::EndTurn
		|| !Controller.IsValid()) return false;
	FBufferedCardIntent Intent;
	if (!Controller->TryCaptureBufferedCardTarget(RuntimeId, Intent)) return false;
	// Every click re-captures the whole credential; never reuse an old target.
	Clear();
	Pending.Kind = EBufferedPlayerIntentKind::CardSelection;
	Pending.Card = Intent;
	return true;
}

EBufferedPlayerIntentEvaluation FBattleHUDBufferedPlayerInput::EvaluatePending()
{
	if (Pending.Kind == EBufferedPlayerIntentKind::None) return EBufferedPlayerIntentEvaluation::None;
	bool bCurrent = HasSafeBinding();
	bool bReady = false;
	if (bCurrent && Pending.Kind == EBufferedPlayerIntentKind::EndTurn)
	{
		FEndTurnIntentAvailability Availability;
		bCurrent = TryGetEndTurnAuthority(Availability) && Availability.Turn == Pending.EndTurn.Turn;
		if (bCurrent && Pending.EndTurn.PresentationFence.IsSet())
		{
			bCurrent = ViewModel->IsPresentationDisplayOwned() && Controller.IsValid()
				&& Controller->IsCurrentPresentationSession(Pending.EndTurn.PresentationFence.GetValue());
		}
		bReady = bCurrent && Availability.bCanExecuteEndTurnNow;
	}
	else if (bCurrent)
	{
		bCurrent = Controller.IsValid() && Controller->IsBufferedCardTargetCurrent(Pending.Card);
		if (bCurrent)
		{
			const FBattleHUDInteractionReadinessShadow Readiness =
				ViewModel->EvaluateInteractionReadinessShadow(Controller.Get());
			bReady = ViewModel->BattleId == Pending.Card.BattleId
				&& ViewModel->StateRevision == Pending.Card.ExpectedReadyRevision
				&& Readiness.bReady && Readiness.Mode == EBattleHUDReadinessMode::NormalPlayerTurn
				&& ViewModel->GetCardPresentationOwner(Pending.Card.RuntimeId) == ECardPresentationOwner::Hand;
		}
	}
	if (!bCurrent)
	{
		Clear();
		return EBufferedPlayerIntentEvaluation::Dropped;
	}
	return bReady ? EBufferedPlayerIntentEvaluation::Ready : EBufferedPlayerIntentEvaluation::Waiting;
}

bool FBattleHUDBufferedPlayerInput::TakeReadyIntent(FBufferedPlayerIntentDecision& OutDecision)
{
	OutDecision = FBufferedPlayerIntentDecision{};
	if (EvaluatePending() != EBufferedPlayerIntentEvaluation::Ready) return false;
	OutDecision = Pending;
	Clear(); // consumed before a future G9-B caller can publish/re-enter a request
	return true;
}
