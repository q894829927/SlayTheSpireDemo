#include "BattleHUDWidgetBase.h"

#include "BattleHUDViewModel.h"
#include "../Battle/BattleManager.h"
#include "../Battle/BattleRequestTypes.h"
#include "../Presentation/BattlePresentationController.h"
#include "Containers/Ticker.h"

void UBattleHUDWidgetBase::ReleaseBufferedPlayerInputBinding()
{
	if (ABattleManager* Battle = BufferedInputBattle.Get())
	{
		Battle->OnReadStateReady.RemoveAll(this);
		Battle->OnPresentationResolutionReady.RemoveAll(this);
	}
	BufferedInputBattle.Reset();
	BufferedPlayerInput.Clear();
	bBufferedInputEvaluationScheduled = false;
	if (++BufferedInputBindingGeneration == 0) ++BufferedInputBindingGeneration;
}

void UBattleHUDWidgetBase::RebindBufferedPlayerInput()
{
	ABattleManager* NewBattle = IsValid(ViewModel) ? ViewModel->BattleManager.Get() : nullptr;
	if (BufferedInputBattle.Get() != NewBattle)
	{
		ReleaseBufferedPlayerInputBinding();
		BufferedInputBattle = NewBattle;
		if (IsValid(NewBattle))
		{
			NewBattle->OnReadStateReady.AddUObject(this, &UBattleHUDWidgetBase::HandleBufferedInputReadReady);
			NewBattle->OnPresentationResolutionReady.AddUObject(this, &UBattleHUDWidgetBase::HandleBufferedInputResolutionReady);
		}
	}
	BufferedPlayerInput.Bind(this);
	BufferedPlayerInput.SetEnabled(bBufferedPlayerInputEnabled);
}

void UBattleHUDWidgetBase::SetBufferedPlayerInputEnabled(bool bEnabled)
{
	bBufferedPlayerInputEnabled = bEnabled;
	RebindBufferedPlayerInput();
	if (bEnabled) BufferedPlayerInput.EnableConfirmedPlayQueue();
	if (!bEnabled)
	{
		RetirePendingFastCardRetry();
		if (IsValid(PresentationController)) PresentationController->CancelTurnEndDiscardGroupPlayback();
		if (IsValid(ViewModel))
		{
			TGuardValue<bool> PreservePlayback(bSuppressPresentationCancellation, true);
			ViewModel->CancelSelection();
		}
		NativeOnBufferedPlayerInputChanged();
	}
	else RetirePendingFastCardRetry();
	NotifyBufferedPlayerInputReadinessChanged();
}

bool UBattleHUDWidgetBase::CanAcceptEndTurnIntent()
{
	RebindBufferedPlayerInput();
	BufferedPlayerInput.EvaluatePending();
	return BufferedPlayerInput.EvaluateEndTurnAvailability().bCanAcceptEndTurnIntent;
}

bool UBattleHUDWidgetBase::HasAcceptedBufferedEndTurn()
{
	if (!bBufferedPlayerInputEnabled) return false;
	RebindBufferedPlayerInput();
	BufferedPlayerInput.EvaluatePending();
	return BufferedPlayerInput.GetPendingKind() == EBufferedPlayerIntentKind::EndTurn;
}

bool UBattleHUDWidgetBase::TryBufferCardSelection(int32 RuntimeId)
{
	if (!bBufferedPlayerInputEnabled) return false;
	RebindBufferedPlayerInput();
	BufferedPlayerInput.EvaluatePending();
	if (!BufferedPlayerInput.BeginCardDraft(RuntimeId))
	{
		if (IsValid(ViewModel))
		{
			TGuardValue<bool> PreservePlayback(bSuppressPresentationCancellation, true);
			ViewModel->ReportQueuedPlayFeedback(NSLOCTEXT("BattleHUD", "QueueDraftRejected", "无法排队：卡牌已入队、队列已满或当前不能出牌"));
		}
		return false;
	}
	RetirePendingFastCardRetry();
	if (IsValid(ViewModel) && !ViewModel->bInputLocked)
	{
		TGuardValue<bool> PreservePlayback(bSuppressPresentationCancellation, true);
		ViewModel->SelectCardByRuntimeId(RuntimeId); // retain caught-up Gameplay preview
	}
	else if (IsValid(ViewModel) && !ViewModel->LastFeedback.IsEmpty())
	{
		TGuardValue<bool> PreservePlayback(bSuppressPresentationCancellation, true);
		ViewModel->ReportQueuedPlayFeedback(FText::GetEmpty());
	}
	NotifyBufferedPlayerInputReadinessChanged();
	return true;
}

void UBattleHUDWidgetBase::HandleBufferedInputReadReady(uint64, uint64)
{
	NotifyBufferedPlayerInputReadinessChanged();
}

void UBattleHUDWidgetBase::HandleBufferedInputResolutionReady(const FPresentationResolutionEnvelope&)
{
	NotifyBufferedPlayerInputReadinessChanged();
}

void UBattleHUDWidgetBase::NotifyBufferedPlayerInputReadinessChanged()
{
	if (!bBufferedPlayerInputEnabled) return;
	RebindBufferedPlayerInput();
	// Invalidation is synchronous, so a mandatory decision cannot disappear
	// before the deferred consumer observes it. Consumption waits until all
	// synchronous Ready/display listeners have published their coherent state.
	BufferedPlayerInput.EvaluatePending();
	NativeOnBufferedPlayerInputChanged();
	if (bBufferedInputEvaluationScheduled) return;
	bBufferedInputEvaluationScheduled = true;
	const uint64 Generation = BufferedInputBindingGeneration;
	const TWeakObjectPtr<UBattleHUDWidgetBase> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[WeakThis, Generation](float)
		{
			if (UBattleHUDWidgetBase* Owner = WeakThis.Get(); Owner && Owner->BufferedInputBindingGeneration == Generation)
			{
				Owner->bBufferedInputEvaluationScheduled = false;
				Owner->ProcessBufferedPlayerInput();
			}
			return false;
		}), 0.0f);
}

void UBattleHUDWidgetBase::ProcessBufferedPlayerInput()
{
	if (!bBufferedPlayerInputEnabled || bBufferedInputProcessing) return;
	TGuardValue<bool> Guard(bBufferedInputProcessing, true);
	TGuardValue<bool> PreservePlayback(bSuppressPresentationCancellation, true);
	RebindBufferedPlayerInput();
	for (int32 Attempt = 0; Attempt <= 32 && IsValid(ViewModel); ++Attempt)
	{
		FBufferedPlayerIntentDecision Decision;
		if (!BufferedPlayerInput.TakeReadyIntent(Decision)) break;
		if (Decision.Kind == EBufferedPlayerIntentKind::CardPlay)
		{
			NativeOnCardPlayRequestStarting(Decision.Play.RuntimeId, &Decision.Play);
			const auto Result = ViewModel->RequestQueuedCardPlay(Decision.Play);
			NativeOnCardPlayRequestFinished(Result.IsAcceptedForResolution());
			if (Result.IsAcceptedForResolution()) break;
			BufferedPlayerInput.RetireRejectedPlayAttempt();
			if (Result.FailureReason == EGameplayRequestFailureReason::BattleEnded
				|| Result.FailureReason == EGameplayRequestFailureReason::InvalidBattle
				|| Result.FailureReason == EGameplayRequestFailureReason::WrongTurn
				|| Result.FailureReason == EGameplayRequestFailureReason::ResolutionFaulted)
			{ BufferedPlayerInput.Clear(); break; }
			if (Result.FailureReason == EGameplayRequestFailureReason::ResolutionBusy)
			{ BufferedPlayerInput.RestoreBusyPlay(Decision.Play); break; }
			continue;
		}
		if (Decision.Kind == EBufferedPlayerIntentKind::EndTurn)
		{
			const bool bAccepted = ViewModel->RequestEndTurnForAcceptedIntent(Decision.EndTurn.Turn);
			BufferedPlayerInput.CompleteEndTurnSubmission(Decision.EndTurn, bAccepted);
		}
		else if (Decision.Kind == EBufferedPlayerIntentKind::CardSelection)
			ViewModel->SelectCardByRuntimeId(Decision.Card.RuntimeId);
		break;
	}
	NativeOnBufferedPlayerInputChanged();
}

EBattleHUDInteractionState UBattleHUDWidgetBase::GetBufferedCardDraftState() const
{
	if (!IsBufferedPlayerInputEnabled() || GetBufferedCardDraftRuntimeId() == INDEX_NONE) return EBattleHUDInteractionState::Idle;
	return BufferedPlayerInput.GetDraftTargetType() == ECardTargetType::None
		? EBattleHUDInteractionState::ReadyToConfirm : EBattleHUDInteractionState::ChoosingTarget;
}

bool UBattleHUDWidgetBase::TryGetBufferedDraftTarget(FName PresentationId, FBattleHUDTargetView& OutTarget) const
{
	return IsBufferedPlayerInputEnabled() && BufferedPlayerInput.TryGetDraftTarget(PresentationId, OutTarget);
}

void UBattleHUDWidgetBase::DiscardQueuedPlayerInput()
{
	BufferedPlayerInput.Clear();
}
