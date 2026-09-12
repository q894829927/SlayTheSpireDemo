#include "BattleHUDViewModel.h"

#include "BattleBufferedPlayerIntent.h"
#include "../Battle/BattleManager.h"
#include "../Presentation/BattlePresentationController.h"

namespace
{
	struct FG9BViewModelInputState
	{
		FBufferedPlayerIntentShadowState Owner;
		TWeakObjectPtr<UBattlePresentationController> CardController;
		TWeakObjectPtr<ABattleManager> OpportunityBattle;
		FDelegateHandle OpportunityHandle;
		bool bRefreshing = false;
	};

	TMap<TWeakObjectPtr<UBattleHUDViewModel>, FG9BViewModelInputState> GInputStates;

	void UnbindOpportunity(FG9BViewModelInputState& State)
	{
		if (ABattleManager* Battle = State.OpportunityBattle.Get();
			IsValid(Battle) && State.OpportunityHandle.IsValid())
		{
			Battle->OnPlayerCommandOpportunity.Remove(State.OpportunityHandle);
		}
		State.OpportunityBattle.Reset();
		State.OpportunityHandle.Reset();
	}

	void RemoveState(UBattleHUDViewModel* ViewModel)
	{
		if (!IsValid(ViewModel))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDViewModel> Key(ViewModel);
		if (FG9BViewModelInputState* State = GInputStates.Find(Key))
		{
			UnbindOpportunity(*State);
		}
		GInputStates.Remove(Key);
	}

	void PruneDeadStates()
	{
		for (auto It = GInputStates.CreateIterator(); It; ++It)
		{
			if (It.Key().IsValid())
			{
				continue;
			}
			UnbindOpportunity(It.Value());
			It.RemoveCurrent();
		}
	}

	FG9BViewModelInputState* FindState(const UBattleHUDViewModel* ViewModel)
	{
		PruneDeadStates();
		if (!IsValid(ViewModel))
		{
			return nullptr;
		}
		return GInputStates.Find(
			TWeakObjectPtr<UBattleHUDViewModel>(const_cast<UBattleHUDViewModel*>(ViewModel)));
	}

	FG9BViewModelInputState& FindOrAddState(UBattleHUDViewModel* ViewModel)
	{
		PruneDeadStates();
		return GInputStates.FindOrAdd(TWeakObjectPtr<UBattleHUDViewModel>(ViewModel));
	}

	void BindEndTurnOpportunity(
		UBattleHUDViewModel* ViewModel,
		ABattleManager* Battle,
		FG9BViewModelInputState& State)
	{
		if (!IsValid(ViewModel) || !IsValid(Battle))
		{
			return;
		}

		if (State.OpportunityBattle.Get() == Battle && State.OpportunityHandle.IsValid())
		{
			return;
		}

		UnbindOpportunity(State);
		State.OpportunityBattle = Battle;
		const TWeakObjectPtr<UBattleHUDViewModel> WeakViewModel(ViewModel);
		State.OpportunityHandle = Battle->OnPlayerCommandOpportunity.AddLambda(
			[WeakViewModel](const FPlayerTurnAuthorityToken& /*OpportunityToken*/)
			{
				if (UBattleHUDViewModel* Current = WeakViewModel.Get())
				{
					Current->RefreshBufferedPlayerIntentG9();
				}
				else
				{
					PruneDeadStates();
				}
			});
	}

	void PublishInputRefresh(UBattleHUDViewModel* ViewModel)
	{
		if (IsValid(ViewModel))
		{
			// G9 pending-intent state is transient input state. Reuse the existing
			// Native change channel rather than inventing a second UI model.
			ViewModel->OnNativeChanged.Broadcast(EBattleHUDDirtyFlags::Input);
			ViewModel->OnChanged.Broadcast();
		}
	}
}

bool UBattleHUDViewModel::StoreBufferedCardSelectionG9(
	const FBufferedCardIntent& Intent,
	UBattlePresentationController* Controller)
{
	if (!Intent.IsValid() || !IsValid(Controller))
	{
		return false;
	}

	FG9BViewModelInputState& State = FindOrAddState(this);
	if (State.Owner.HasEndTurn() || !State.Owner.StoreCardSelection(Intent))
	{
		return false;
	}

	State.CardController = Controller;
	UnbindOpportunity(State);
	return true;
}

bool UBattleHUDViewModel::HasBufferedCardSelectionG9() const
{
	const FG9BViewModelInputState* State = FindState(this);
	return State != nullptr && State->Owner.HasCardSelection();
}

bool UBattleHUDViewModel::HasBufferedEndTurnG9() const
{
	const FG9BViewModelInputState* State = FindState(this);
	return State != nullptr && State->Owner.HasEndTurn();
}

bool UBattleHUDViewModel::CanAcceptEndTurnIntentG9() const
{
	if (Outcome != EBattleHUDOutcome::None
		|| InteractionState == EBattleHUDInteractionState::Terminal
		|| InteractionState == EBattleHUDInteractionState::PresentationUnavailable
		|| HasBufferedEndTurnG9())
	{
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	FBufferedEndTurnIntent Probe;
	return IsValid(Battle) && TryCaptureBufferedEndTurnShadow(Battle, Probe);
}

bool UBattleHUDViewModel::TryAcceptEndTurnIntentG9()
{
	if (!CanAcceptEndTurnIntentG9())
	{
		return false;
	}

	ABattleManager* Battle = BattleManager.Get();
	FBufferedEndTurnIntent Captured;
	if (!IsValid(Battle) || !TryCaptureBufferedEndTurnShadow(Battle, Captured))
	{
		return false;
	}

	Captured.CaptureStateRevision = StateRevision;
	FG9BViewModelInputState& State = FindOrAddState(this);
	if (!State.Owner.StoreEndTurn(Captured))
	{
		return false;
	}

	// StoreEndTurn atomically retires any older card future intent.
	State.CardController.Reset();
	BindEndTurnOpportunity(this, Battle, State);
	return true;
}

bool UBattleHUDViewModel::FinalizeAcceptedEndTurnIntentG9()
{
	FG9BViewModelInputState* State = FindState(this);
	if (State == nullptr || !State->Owner.HasEndTurn() || State->bRefreshing)
	{
		return false;
	}

	TGuardValue<bool> RefreshGuard(State->bRefreshing, true);

	// G9-B scoped supersede: only after the exact EndTurn intent is accepted may
	// it retire a cancelable transient card selection.
	if (InteractionState == EBattleHUDInteractionState::ReadyToConfirm
		|| InteractionState == EBattleHUDInteractionState::ChoosingTarget)
	{
		CancelSelection();
	}

	State = FindState(this);
	if (State == nullptr || !State->Owner.HasEndTurn())
	{
		return false;
	}

	const FBufferedEndTurnIntent* Pending = State->Owner.GetEndTurn();
	ABattleManager* Battle = BattleManager.Get();
	if (Pending == nullptr || !IsValid(Battle))
	{
		RemoveState(this);
		return false;
	}

	const EBufferedIntentShadowEvaluation Evaluation =
		EvaluateBufferedEndTurnShadow(Battle, *Pending);
	if (Evaluation == EBufferedIntentShadowEvaluation::Waiting)
	{
		BindEndTurnOpportunity(this, Battle, *State);
		PublishInputRefresh(this);
		return true;
	}
	if (Evaluation != EBufferedIntentShadowEvaluation::Ready)
	{
		RemoveState(this);
		PublishInputRefresh(this);
		return false;
	}

	FBufferedEndTurnIntent Consumed;
	if (!State->Owner.TryTakeEndTurn(Consumed))
	{
		RemoveState(this);
		return false;
	}
	RemoveState(this);

	// Consume before the authoritative request so a synchronous state transition
	// can never replay this physical click into a later player turn.
	const FGameplayRequestResult Result = Battle->RequestEndPlayerTurn();
	if (!Result.IsAcceptedForResolution())
	{
		SetFeedback(Result.FailureReason);
		BroadcastChanged(EBattleHUDDirtyFlags::Feedback | EBattleHUDDirtyFlags::Input);
		return false;
	}

	ClearSelectionInternal();
	ClearFeedback();
	ClearLiveInputBindings();
	SetResolving();
	BroadcastChanged(
		EBattleHUDDirtyFlags::Input
		| EBattleHUDDirtyFlags::Combatants
		| EBattleHUDDirtyFlags::Feedback);
	return true;
}

void UBattleHUDViewModel::RefreshBufferedPlayerIntentG9()
{
	FG9BViewModelInputState* State = FindState(this);
	if (State == nullptr || State->bRefreshing)
	{
		return;
	}

	if (State->Owner.HasEndTurn())
	{
		FinalizeAcceptedEndTurnIntentG9();
		return;
	}

	if (!State->Owner.HasCardSelection())
	{
		RemoveState(this);
		return;
	}

	UBattlePresentationController* Controller = State->CardController.Get();
	const FBufferedCardIntent* Pending = State->Owner.GetCardSelection();
	if (!IsValid(Controller) || Pending == nullptr)
	{
		RemoveState(this);
		return;
	}

	const EBufferedIntentShadowEvaluation Evaluation =
		Controller->EvaluateBufferedCardTarget(*Pending);
	if (Evaluation == EBufferedIntentShadowEvaluation::Waiting)
	{
		return;
	}
	if (Evaluation != EBufferedIntentShadowEvaluation::Ready)
	{
		RemoveState(this);
		return;
	}

	FBufferedCardIntent Consumed;
	if (!State->Owner.TryTakeCardSelection(Consumed))
	{
		RemoveState(this);
		return;
	}
	RemoveState(this);

	// Consume first, then enter the ordinary selection state exactly once. This
	// old physical click never auto-confirms or auto-targets.
	SelectCardByRuntimeId(Consumed.RuntimeId);
}

void UBattleHUDViewModel::ClearBufferedPlayerIntentG9()
{
	RemoveState(this);
}
