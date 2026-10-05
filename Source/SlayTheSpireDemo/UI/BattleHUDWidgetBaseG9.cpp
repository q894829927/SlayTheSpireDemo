#include "BattleHUDWidgetBase.h"

#include "BattleHUDViewModel.h"
#include "../Battle/BattleManager.h"
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
	if (!bEnabled)
	{
		RetirePendingFastCardRetry();
		NativeOnBufferedPlayerInputChanged();
	}
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
	if (!BufferedPlayerInput.TryCaptureCard(RuntimeId)) return false;
	RetirePendingFastCardRetry();
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
	FBufferedPlayerIntentDecision Decision;
	if (BufferedPlayerInput.TakeReadyIntent(Decision) && IsValid(ViewModel))
	{
		if (Decision.Kind == EBufferedPlayerIntentKind::EndTurn)
			ViewModel->RequestEndTurnForAcceptedIntent(Decision.EndTurn.Turn);
		else if (Decision.Kind == EBufferedPlayerIntentKind::CardSelection)
			ViewModel->SelectCardByRuntimeId(Decision.Card.RuntimeId);
	}
	NativeOnBufferedPlayerInputChanged();
}
