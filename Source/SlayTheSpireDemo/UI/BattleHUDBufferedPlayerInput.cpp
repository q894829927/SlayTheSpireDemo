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
		SubmittedEndTurn.Reset();
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
	// Clear is also an input lifetime boundary, even if Session/turn are unchanged.
	if (++BindingGeneration == 0) ++BindingGeneration;
	Pending = FBufferedPlayerIntentDecision{};
	Draft.Reset();
	ConfirmedPlays.Reset();
	bAwaitingSubmittedPlay = false;
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
	Result.bCanAcceptEndTurnIntent &= !IsSubmittedEndTurnBlockingInput(Result.Turn);
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
	Result.bRetireCardIntent = Pending.Kind == EBufferedPlayerIntentKind::CardSelection || Draft.IsSet();
	Result.bRetireFastInputRetry = bHasPendingFastInputRetry;
	Result.bCancelTransientSelection = ViewModel->InteractionState == EBattleHUDInteractionState::ReadyToConfirm
		|| ViewModel->InteractionState == EBattleHUDInteractionState::ChoosingTarget;
	Draft.Reset();
	// This is an ordering barrier: keep earlier confirmed/popped commands and
	// their generation. Only later additions and the unconfirmed draft retire.
	Pending.Kind = EBufferedPlayerIntentKind::EndTurn;
	Pending.EndTurn = Intent;
	return Result;
}

bool FBattleHUDBufferedPlayerInput::TryCaptureCard(int32 RuntimeId)
{
	FPlayerTurnAuthorityToken Turn;
	if (!HasSafeBinding() || Pending.Kind == EBufferedPlayerIntentKind::EndTurn
		|| !Controller.IsValid() || !Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn)
		|| IsSubmittedEndTurnBlockingInput(Turn)) return false;
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
	RetireSubmittedEndTurnAtReadyBoundary();
	if (bConfirmedQueueMode)
	{
		if (bAwaitingSubmittedPlay && HasSafeBinding() && IsNormalSubmissionReady()) bAwaitingSubmittedPlay = false;
		bool bCurrent = HasSafeBinding() && !ViewModel->HasAuthoritativePendingCardSelection();
		if (bCurrent && Draft.IsSet()) bCurrent = IsQueuedIdentityCurrent(Draft.GetValue());
		for (const FQueuedCardPlayIntent& Intent : ConfirmedPlays)
			if (bCurrent) bCurrent = IsQueuedIdentityCurrent(Intent);
		if (!bCurrent) { Clear(); return EBufferedPlayerIntentEvaluation::Dropped; }
		if (Draft.IsSet() && !ViewModel->HandCards.ContainsByPredicate([this](const FBattleHUDCardView& Card)
			{ return Card.RuntimeId == Draft->RuntimeId && Card.CardId == Draft->CardId; })) Draft.Reset();
		if (!ConfirmedPlays.IsEmpty())
			return IsNormalSubmissionReady() ? EBufferedPlayerIntentEvaluation::Ready : EBufferedPlayerIntentEvaluation::Waiting;
	}
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
		bReady = bCurrent && Availability.bCanExecuteEndTurnNow && !bAwaitingSubmittedPlay;
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
	if (!ConfirmedPlays.IsEmpty())
	{
		OutDecision.Kind = EBufferedPlayerIntentKind::CardPlay;
		OutDecision.Play = ConfirmedPlays[0];
		ConfirmedPlays.RemoveAt(0); // pop BEFORE the request can publish/re-enter
		bAwaitingSubmittedPlay = true;
		return true;
	}
	OutDecision = Pending;
	if (Pending.Kind == EBufferedPlayerIntentKind::EndTurn) SubmittedEndTurn = Pending.EndTurn;
	Pending = FBufferedPlayerIntentDecision{};
	return true;
}

void FBattleHUDBufferedPlayerInput::RetireSubmittedEndTurnAtReadyBoundary()
{
	if (!SubmittedEndTurn.IsSet() || !HasSafeBinding()) return;
	FPlayerTurnAuthorityToken Turn;
	if (Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn)
		&& Turn.BattleId == SubmittedEndTurn->Turn.BattleId
		&& Turn != SubmittedEndTurn->Turn && IsNormalSubmissionReady())
	{
		// A proven next-turn Ready edge completes this fence's lifetime once.
		// Later card playback must not reactivate a completed prior-turn fence.
		SubmittedEndTurn.Reset();
	}
}

bool FBattleHUDBufferedPlayerInput::IsSubmittedEndTurnBlockingInput(const FPlayerTurnAuthorityToken& Turn) const
{
	return SubmittedEndTurn.IsSet() && SubmittedEndTurn->Turn.BattleId == Turn.BattleId;
}

void FBattleHUDBufferedPlayerInput::CompleteEndTurnSubmission(const FBufferedEndTurnIntent& Intent, bool bAccepted)
{
	if (!bAccepted && SubmittedEndTurn.IsSet()
		&& SubmittedEndTurn->LocalIntentGeneration == Intent.LocalIntentGeneration
		&& SubmittedEndTurn->Turn == Intent.Turn) SubmittedEndTurn.Reset();
}

bool FBattleHUDBufferedPlayerInput::IsQueuedIdentityCurrent(const FQueuedCardPlayIntent& Intent) const
{
	FPlayerTurnAuthorityToken Turn;
	return HasSafeBinding() && Intent.BindingGeneration == BindingGeneration
		&& Intent.InputSequence != 0 && Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn)
		&& Turn == Intent.Turn && ViewModel->BattleId == static_cast<int64>(Turn.BattleId)
		&& (!Intent.PresentationFence.IsSet() || (Controller.IsValid()
			&& Controller->IsCurrentPresentationSession(Intent.PresentationFence.GetValue())));
}

bool FBattleHUDBufferedPlayerInput::IsNormalSubmissionReady() const
{
	const auto Ready = ViewModel->EvaluateInteractionReadinessShadow(Controller.Get());
	return Ready.bReady && (Ready.Mode == EBattleHUDReadinessMode::NormalPlayerTurn
		|| Ready.Mode == EBattleHUDReadinessMode::CardTargetChoice
		|| Ready.Mode == EBattleHUDReadinessMode::CardReadyToConfirm);
}

bool FBattleHUDBufferedPlayerInput::CanBeginCardDraft(int32 RuntimeId) const
{
	FPlayerTurnAuthorityToken Turn;
	if (!HasSafeBinding() || Pending.Kind == EBufferedPlayerIntentKind::EndTurn
		|| ViewModel->HasAuthoritativePendingCardSelection() || ConfirmedPlays.Num() >= 32
		|| !Battle->TryGetCurrentPlayerTurnAuthorityToken(Turn)
		|| IsSubmittedEndTurnBlockingInput(Turn)
		|| ViewModel->BattleId != static_cast<int64>(Turn.BattleId)
		|| ViewModel->GetCardPresentationOwner(RuntimeId) != ECardPresentationOwner::Hand
		|| ConfirmedPlays.ContainsByPredicate([RuntimeId](const auto& I) { return I.RuntimeId == RuntimeId; })) return false;
	const auto* Card = ViewModel->HandCards.FindByPredicate([RuntimeId](const FBattleHUDCardView& C) { return C.RuntimeId == RuntimeId; });
	if (!Card || Card->CardId.IsNone() || Card->Cost < 0
		|| (Card->TargetType != ECardTargetType::None && Card->TargetType != ECardTargetType::Enemy && Card->TargetType != ECardTargetType::Self)) return false;
	// Frozen latest membership excludes an already-committed play still visible
	// at its CardPlayed cursor. This is an immutable read, never a binding refresh.
	FPresentationStateSnapshot Latest;
	return Battle->TryGetLatestFrozenPresentationBaseline(Latest) && Latest.BattleId == ViewModel->BattleId
		&& Latest.HandCards.ContainsByPredicate([Card](const FBattleHUDCardView& C) { return C.RuntimeId == Card->RuntimeId && C.CardId == Card->CardId; });
}

bool FBattleHUDBufferedPlayerInput::BeginCardDraft(int32 RuntimeId)
{
	if (!CanBeginCardDraft(RuntimeId)) return false;
	if (GetDraftRuntimeId() == RuntimeId) { Draft.Reset(); return true; }
	FQueuedCardPlayIntent Intent;
	Battle->TryGetCurrentPlayerTurnAuthorityToken(Intent.Turn);
	Intent.BindingGeneration = BindingGeneration;
	Intent.InputSequence = NextIntentGeneration++;
	if (NextIntentGeneration == 0) ++NextIntentGeneration;
	Intent.CaptureStateRevision = ViewModel->StateRevision;
	Intent.RuntimeId = RuntimeId;
	const auto& Card = *ViewModel->HandCards.FindByPredicate([RuntimeId](const auto& C) { return C.RuntimeId == RuntimeId; });
	Intent.CardId = Card.CardId; Intent.TargetType = Card.TargetType;
	if (ViewModel->IsPresentationDisplayOwned())
	{
		FPresentationSessionToken Session;
		if (!Controller.IsValid() || !Controller->TryGetPresentationSessionToken(Session)) return false;
		Intent.PresentationFence = Session;
	}
	Draft = Intent;
	return true;
}

bool FBattleHUDBufferedPlayerInput::TryGetDraftTarget(FName PresentationId, FBattleHUDTargetView& OutTarget) const
{
	OutTarget = FBattleHUDTargetView{};
	if (!Draft.IsSet() || !IsQueuedIdentityCurrent(Draft.GetValue()) || ViewModel->HasAuthoritativePendingCardSelection()) return false;
	const FBattleHUDCombatantView* Target = Draft->TargetType == ECardTargetType::Enemy ? &ViewModel->Enemy
		: Draft->TargetType == ECardTargetType::Self ? &ViewModel->Player : nullptr;
	if (!Target || Target->bDead || Target->HP <= 0 || PresentationId.IsNone() || Target->PresentationId != PresentationId) return false;
	OutTarget.PresentationId = PresentationId;
	OutTarget.TargetId = Target->bPlayer ? 1 : 2; // frozen HUD handle, never a Gameplay identity
	OutTarget.bPlayer = Target->bPlayer; OutTarget.DisplayName = Target->DisplayName;
	return true;
}

bool FBattleHUDBufferedPlayerInput::ConfirmCardDraft(FName TargetPresentationId, const TOptional<FCardPlayVisualOrigin>& Origin)
{
	if (!Draft.IsSet() || !IsQueuedIdentityCurrent(Draft.GetValue()) || Pending.Kind == EBufferedPlayerIntentKind::EndTurn
		|| ViewModel->HasAuthoritativePendingCardSelection() || ConfirmedPlays.Num() >= 32) return false;
	if (Draft->TargetType == ECardTargetType::None) { if (!TargetPresentationId.IsNone()) return false; }
	else { FBattleHUDTargetView Target; if (!TryGetDraftTarget(TargetPresentationId, Target)) return false; }
	Draft->TargetPresentationId = TargetPresentationId;
	// Resting/arrow cards use their current Hand position when submitted. Only
	// pointer-confirmed cards retain a frozen pose during the FIFO wait.
	Draft->VisualOrigin = Origin.IsSet() && Origin->bPointerHeld ? Origin : TOptional<FCardPlayVisualOrigin>{};
	ConfirmedPlays.Add(Draft.GetValue());
	Draft.Reset();
	return true;
}

bool FBattleHUDBufferedPlayerInput::RestoreBusyPlay(const FQueuedCardPlayIntent& Intent)
{
	bAwaitingSubmittedPlay = false;
	if (!IsQueuedIdentityCurrent(Intent) || (Pending.Kind == EBufferedPlayerIntentKind::EndTurn
		&& Intent.InputSequence >= Pending.EndTurn.LocalIntentGeneration)
		|| IsSubmittedEndTurnBlockingInput(Intent.Turn)
		|| ViewModel->HasAuthoritativePendingCardSelection() || ConfirmedPlays.Num() >= 32) return false;
	ConfirmedPlays.Insert(Intent, 0);
	return true;
}
