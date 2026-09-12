#include "BattleHUDWidgetG9BInput.h"

#include "BattleHUDWidget.h"
#include "BattleHUDViewModel.h"
#include "../Presentation/BattlePresentationController.h"
#include "BattleBufferedPlayerIntent.h"
#include "Containers/Ticker.h"

namespace
{
	struct FG9BBufferedCardState
	{
		FBufferedCardIntent Intent;
		TWeakObjectPtr<UBattlePresentationController> Controller;
		TWeakObjectPtr<UBattleHUDViewModel> BoundViewModel;
		FDelegateHandle ViewModelChangedHandle;
		bool bReplayScheduled = false;
	};

	TMap<TWeakObjectPtr<UBattleHUDWidget>, FG9BBufferedCardState> GBufferedCardStates;

	void RemoveState(UBattleHUDWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
		if (FG9BBufferedCardState* State = GBufferedCardStates.Find(Key))
		{
			if (UBattleHUDViewModel* BoundViewModel = State->BoundViewModel.Get();
				IsValid(BoundViewModel) && State->ViewModelChangedHandle.IsValid())
			{
				BoundViewModel->OnNativeChanged.Remove(State->ViewModelChangedHandle);
			}
		}
		GBufferedCardStates.Remove(Key);
	}

	void PruneDeadStates()
	{
		for (auto It = GBufferedCardStates.CreateIterator(); It; ++It)
		{
			if (It.Key().IsValid())
			{
				continue;
			}

			FG9BBufferedCardState& State = It.Value();
			if (UBattleHUDViewModel* BoundViewModel = State.BoundViewModel.Get();
				IsValid(BoundViewModel) && State.ViewModelChangedHandle.IsValid())
			{
				BoundViewModel->OnNativeChanged.Remove(State.ViewModelChangedHandle);
			}
			It.RemoveCurrent();
		}
	}

	bool IsNormalCardInputAvailableNow(const UBattleHUDViewModel* ViewModel)
	{
		return IsValid(ViewModel)
			&& ViewModel->Outcome == EBattleHUDOutcome::None
			&& !ViewModel->bInputLocked
			&& ViewModel->InteractionState != EBattleHUDInteractionState::Resolving
			&& ViewModel->InteractionState != EBattleHUDInteractionState::Terminal
			&& ViewModel->InteractionState != EBattleHUDInteractionState::PresentationUnavailable;
	}

	void ProcessScheduledReplay(const TWeakObjectPtr<UBattleHUDWidget>& WeakWidget)
	{
		UBattleHUDWidget* Widget = WeakWidget.Get();
		if (!IsValid(Widget))
		{
			PruneDeadStates();
			return;
		}

		FG9BBufferedCardState* State = GBufferedCardStates.Find(WeakWidget);
		if (State == nullptr)
		{
			return;
		}
		State->bReplayScheduled = false;

		UBattlePresentationController* Controller = State->Controller.Get();
		if (!IsValid(Controller))
		{
			RemoveState(Widget);
			return;
		}

		const EBufferedIntentShadowEvaluation Evaluation =
			Controller->EvaluateBufferedCardTarget(State->Intent);
		if (Evaluation == EBufferedIntentShadowEvaluation::Waiting)
		{
			return;
		}
		if (Evaluation != EBufferedIntentShadowEvaluation::Ready)
		{
			RemoveState(Widget);
			return;
		}

		const int32 RuntimeId = State->Intent.RuntimeId;
		RemoveState(Widget);

		// One old physical click crosses only the Presentation-lag boundary. The
		// replay enters the ordinary SelectCard contract and does not auto-confirm
		// or auto-target the selected card.
		Widget->SelectCard(RuntimeId, false);
	}

	void ScheduleExactReplayCheck(UBattleHUDWidget* Widget)
	{
		if (!IsValid(Widget))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
		FG9BBufferedCardState* State = GBufferedCardStates.Find(Key);
		if (State == nullptr || State->bReplayScheduled)
		{
			return;
		}
		State->bReplayScheduled = true;

		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[Key](float /*DeltaSeconds*/)
				{
					ProcessScheduledReplay(Key);
					return false;
				}),
			0.0f);
	}

	void BindViewModelWakeup(UBattleHUDWidget* Widget, FG9BBufferedCardState& State)
	{
		UBattleHUDViewModel* DesiredViewModel = IsValid(Widget) ? Widget->ViewModel.Get() : nullptr;
		if (State.BoundViewModel.Get() == DesiredViewModel && State.ViewModelChangedHandle.IsValid())
		{
			return;
		}

		if (UBattleHUDViewModel* Previous = State.BoundViewModel.Get();
			IsValid(Previous) && State.ViewModelChangedHandle.IsValid())
		{
			Previous->OnNativeChanged.Remove(State.ViewModelChangedHandle);
		}
		State.BoundViewModel = DesiredViewModel;
		State.ViewModelChangedHandle.Reset();

		if (!IsValid(DesiredViewModel))
		{
			return;
		}

		const TWeakObjectPtr<UBattleHUDWidget> WeakWidget(Widget);
		State.ViewModelChangedHandle = DesiredViewModel->OnNativeChanged.AddLambda(
			[WeakWidget](EBattleHUDDirtyFlags /*DirtyFlags*/)
			{
				if (UBattleHUDWidget* CurrentWidget = WeakWidget.Get())
				{
					BattleHUDWidgetG9BInput::RefreshBufferedCardIntent(CurrentWidget);
				}
				else
				{
					PruneDeadStates();
				}
			});
	}
}

EG9BCardClickDisposition BattleHUDWidgetG9BInput::TryHandleBufferedCardClick(
	UBattleHUDWidget* Widget,
	int32 RuntimeId)
{
	PruneDeadStates();
	if (!IsValid(Widget) || RuntimeId == INDEX_NONE || !IsValid(Widget->ViewModel))
	{
		return EG9BCardClickDisposition::NotHandled;
	}

	UBattleHUDViewModel* ViewModel = Widget->ViewModel.Get();
	if (IsNormalCardInputAvailableNow(ViewModel))
	{
		RemoveState(Widget);
		return EG9BCardClickDisposition::NotHandled;
	}

	UBattlePresentationController* Controller = Widget->PresentationController.Get();
	FBufferedCardIntent Captured;
	if (!IsValid(Controller)
		|| !Controller->TryCaptureBufferedCardTarget(RuntimeId, Captured))
	{
		// Every physical click re-captures authority. If the new click cannot prove
		// an exact G9 window, do not keep an older credential alive behind it.
		RemoveState(Widget);
		return EG9BCardClickDisposition::NotHandled;
	}

	const TWeakObjectPtr<UBattleHUDWidget> Key(Widget);
	FG9BBufferedCardState& State = GBufferedCardStates.FindOrAdd(Key);
	State.Intent = Captured;
	State.Controller = Controller;
	BindViewModelWakeup(Widget, State);
	return EG9BCardClickDisposition::Buffered;
}

void BattleHUDWidgetG9BInput::RefreshBufferedCardIntent(UBattleHUDWidget* Widget)
{
	PruneDeadStates();
	if (!IsValid(Widget))
	{
		return;
	}

	FG9BBufferedCardState* State = GBufferedCardStates.Find(TWeakObjectPtr<UBattleHUDWidget>(Widget));
	if (State == nullptr)
	{
		return;
	}

	UBattlePresentationController* Controller = State->Controller.Get();
	if (!IsValid(Controller))
	{
		ScheduleExactReplayCheck(Widget);
		return;
	}

	const EBufferedIntentShadowEvaluation Evaluation =
		Controller->EvaluateBufferedCardTarget(State->Intent);
	if (Evaluation != EBufferedIntentShadowEvaluation::Waiting)
	{
		// Defer replay/drop outside the synchronous ViewModel multicast stack.
		ScheduleExactReplayCheck(Widget);
	}
}

void BattleHUDWidgetG9BInput::ClearBufferedCardIntent(UBattleHUDWidget* Widget)
{
	PruneDeadStates();
	RemoveState(Widget);
}

bool BattleHUDWidgetG9BInput::HasBufferedCardIntent(const UBattleHUDWidget* Widget)
{
	PruneDeadStates();
	if (!IsValid(Widget))
	{
		return false;
	}
	return GBufferedCardStates.Contains(
		TWeakObjectPtr<UBattleHUDWidget>(const_cast<UBattleHUDWidget*>(Widget)));
}
